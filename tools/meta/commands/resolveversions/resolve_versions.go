package resolveversions

import (
	"context"
	"fmt"
	"meta/versions/libmpdclientver"
	"meta/versions/mpdver"
	"sort"
	"strings"

	"github.com/urfave/cli/v3"
)

func resolve(ctx context.Context, cmd *cli.Command) error {
	var out []string

	{
		v, err := mpdver.Resolve(cmd.String("mpd"))
		if err != nil {
			return err
		}
		out = append(out, "MPD_VERSION="+v.String())
	}

	{
		v, err := libmpdclientver.Resolve(cmd.String("libmpdclient"))
		if err != nil {
			return err
		}
		out = append(out, "LIBMPDCLIENT_VERSION="+v.String())
	}

	sort.Strings(out)
	fmt.Println(strings.Join(out, "\n"))

	return nil
}

var Command = &cli.Command{
	Name:  "resolve-versions",
	Usage: "resolve-versions --mpd latest --libmpdclient latest",
	Flags: []cli.Flag{
		&cli.StringFlag{
			Name:  "mpd",
			Value: "latest",
			Usage: strings.Join([]string{
				"version of mpd to resolve, or 'latest' to",
				"automatically query for the latest released version, and",
				"resolve that.",
			}, " "),
		},
		&cli.StringFlag{
			Name:  "libmpdclient",
			Value: "latest",
			Usage: strings.Join([]string{
				"version of libmpdclient to resolve, or 'latest' to",
				"automatically query for the latest released version.",
			}, " "),
		},
	},
	Action: resolve,
}
