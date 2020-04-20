package main

import (
	"fmt"
	"os"

	"github.com/urfave/cli/v2"

	"meta/commands/libmpdclient"
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
	app := &cli.App{
		Commands: []*cli.Command{
			{
				Name: "install",
				Subcommands: []*cli.Command{
					libmpdclient.Command,
					{
						Name:   "mpd",
						Action: doMPD,
					},
				},
			},
		},
	}
	app.Run(os.Args)
}
