//go:build !windows

package main

import (
	"os/exec"
	"runtime"
	"strconv"
	"strings"
)

func quietHelper(cmd *exec.Cmd) {}
func preparationSupported() bool {
	if runtime.GOOS != "darwin" {
		return true
	}
	// The bundled extraction tools require macOS 13, although the game runs on 11+.
	version, err := exec.Command("/usr/bin/uname", "-r").Output()
	major, parse := strconv.Atoi(strings.Split(strings.TrimSpace(string(version)), ".")[0])
	return err == nil && parse == nil && major >= 22
}
