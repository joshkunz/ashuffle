package main

import (
	"context"
	"log"
	"os"

	"github.com/urfave/cli/v3"

	"meta/commands/libmpdclient"
	"meta/commands/mpd"
	"meta/commands/release"
	"meta/commands/resolveversions"
	"meta/commands/testbuild"
)

func main() {
	log.SetOutput(os.Stderr)
	app := &cli.Command{
		Commands: []*cli.Command{
			{
				Name: "install",
				Commands: []*cli.Command{
					libmpdclient.Command,
					mpd.Command,
				},
			},
			resolveversions.Command,
			release.Command,
			testbuild.Command,
		},
	}
	if err := app.Run(context.Background(), os.Args); err != nil {
		log.Fatal(err)
	}
}
