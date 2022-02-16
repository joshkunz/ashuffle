package main

import (
	"fmt"
	"log"
	"os"

	"github.com/urfave/cli/v2"

	"meta/commands/libmpdclient"
	"meta/commands/mpd"
	"meta/commands/release"
	"meta/commands/testbuild"
)

func doLibmpdclient(ctx *cli.Context) error {
	fmt.Printf("blah libmpdclient\n")
	return nil
}

func doMPD(ctx *cli.Context) error {
	fmt.Printf("blah mpd\n")
	return nil
}

func main() {
	log.SetOutput(os.Stderr)
	app := &cli.App{
		Commands: []*cli.Command{
			{
				Name: "install",
				Subcommands: []*cli.Command{
					libmpdclient.Command,
					mpd.Command,
				},
			},
			release.Command,
			testbuild.Command,
		},
	}
	if err := app.Run(os.Args); err != nil {
		log.Fatal(err)
	}
}
