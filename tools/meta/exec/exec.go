package exec

import (
	"log"
	"os"
	"os/exec"
)

type Cmd struct {
	*exec.Cmd

	silent bool
}

func (c *Cmd) Run() error {
	if !c.silent {
		log.Printf("+ %s", c)
	}
	return c.Cmd.Run()
}

func Command(path string, args ...string) *Cmd {
	c := exec.Command(path, args...)
	c.Stdout = os.Stdout
	c.Stderr = os.Stderr
	return &Cmd{Cmd: c}
}

func Silent(path string, args ...string) *Cmd {
	c := exec.Command(path, args...)
	return &Cmd{Cmd: c, silent: true}
}
