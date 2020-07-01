If your distributions package provides the service file, you can start the ashuffle service with:

    $ systemctl --user start ashuffle.service

use systemctl enable to autostart ashuffle on every login:

    $ systemctl --user enable ashuffle.service


If your installed package doesn't provide the service file, just copy it into your systemd user directory before using the above commands:

    ~/.config/systemd/user/ashuffle.service



To start the ashuffle service with parameters, use the systemctl edit command:

    systemctl --user edit ashuffle.service

this creates and opens the file ~/.config/systemd/user/ashuffle.service.d/override.conf, insert something like this:
```
    [Service]
    ExecStart=
    ExecStart=/usr/bin/ashuffle --only 10 --tweak play-on-startup=no
```

Note how ExecStart must be cleared before being re-assigned.
