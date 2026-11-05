package main

import (
	"os/exec"
	"syscall"
)

func quietHelper(cmd *exec.Cmd)  { cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true} }
func preparationSupported() bool { return true }
