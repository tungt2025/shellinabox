#!/bin/sh
if [ -n "$NEW_USER" ] && [ -n "$NEW_PASS" ]; then
  if id "$NEW_USER" 2>/dev/null; then
    echo "$NEW_USER exists, updating password."
    echo "$NEW_USER:$NEW_PASS" | chpasswd
  else
    echo "Creating user $NEW_USER."
    useradd -m -s /bin/bash "$NEW_USER"
    echo "$NEW_USER:$NEW_PASS" | chpasswd
  fi
fi
exec "$@"
