package integration_test

import (
    "testing"
    "mpd"
)

func TestTest(t *testing.T) {
    opts := mpd.Options{
        LibraryRoot: "/music",
        DefaultPermissions: []string{"read", "play", "queue"},
        Passwords: []mpd.Password{
            {
                Password: "bad_password",
                Permissions: []string{"read"},
            },
            {
                Password: "bad_password2",
                Permissions: []string{"read", "play"},
            },
        },
    }
    cfg, _ := opts.Build("testRoot")
    t.Errorf("%s\n", cfg) 
}
