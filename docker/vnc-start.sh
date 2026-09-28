#!/usr/bin/env bash
# Starts a virtual X server (Xvfb) and a VNC server inside THIS container,
# then runs whatever command was given with DISPLAY pointed at that virtual
# server. Baked into the image as /usr/local/bin/vnc-start.sh; run.sh
# prepends it to the container's command when launched with --vnc.
#
# Why this exists, instead of just using XQuartz: XQuartz bridges GLX over
# the network (DISPLAY=host.docker.internal:0), and that depends on Mesa's
# indirect/software GLX rendering path — which modern Mesa (this image ships
# Ubuntu 22.04's ~22.x) no longer supports. Confirmed on a real run,
# 2026-09-28: ANY glX context request over that network DISPLAY comes back
# "X_GLXCreateNewContext / BadValue", core profile or not — glxinfo itself
# fails the same way, which rules out "wrong GL version" as the cause.
#
# Xvfb sidesteps this by being a real X server that lives INSIDE this
# container, so the connection to it is local (a Unix domain socket, not a
# network one) — the same situation `xvfb-run` puts any headless GL program
# in for CI, and Mesa's software rendering (llvmpipe) works completely
# normally there. Only the finished PIXELS leave the container, over VNC (a
# plain framebuffer-streaming protocol with no GL version negotiation to
# break), to whatever VNC client you point at vnc://localhost:$VNC_PORT —
# macOS's own Screen Sharing app is enough, no extra install needed.
set -euo pipefail

VNC_DISPLAY="${VNC_DISPLAY:-:99}"
VNC_RESOLUTION="${VNC_RESOLUTION:-1280x720x24}"
VNC_PORT="${VNC_PORT:-5900}"

Xvfb "$VNC_DISPLAY" -screen 0 "$VNC_RESOLUTION" -nolisten tcp &

# Xvfb forks and returns before the display socket necessarily exists yet —
# wait for it rather than racing x11vnc's connection attempt against it.
SOCK="/tmp/.X11-unix/X${VNC_DISPLAY#:}"
i=0
while [ ! -e "$SOCK" ]; do
	i=$((i + 1))
	if [ "$i" -ge 50 ]; then
		echo "vnc-start: Xvfb never created $SOCK — aborting" >&2
		exit 1
	fi
	sleep 0.1
done

x11vnc -display "$VNC_DISPLAY" -forever -shared -nopw -quiet \
	-rfbport "$VNC_PORT" -bg -o /tmp/x11vnc.log

export DISPLAY="$VNC_DISPLAY"
echo ">> VNC ready: connect from your Mac to vnc://localhost:$VNC_PORT" >&2
echo ">> (Finder -> Go -> Connect to Server -> that address, or any VNC viewer)" >&2

# --rm means the whole container (Xvfb and x11vnc included) is torn down the
# moment this exits, so there is nothing else here to clean up on the way out.
if [ "$#" -eq 0 ]; then
	exec /bin/bash
else
	exec "$@"
fi
