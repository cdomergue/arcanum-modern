package main

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"
)

func TestLauncherLogRotationAndFailure(t *testing.T) {
	root := t.TempDir()
	l := newProcessLog(root, "launcher-ui.log")
	data := bytes.Repeat([]byte("x"), processLogLimit*3+17)
	if n, err := l.Write(data); n != len(data) || err != nil {
		t.Fatal(n, err)
	}
	l.event("exit", "code=0xffffffff")
	l.Close()
	current, err := os.ReadFile(filepath.Join(root, "ArcanumModern", "launcher-ui.log"))
	if err != nil || !bytes.Contains(current, []byte("process_exit code=0xffffffff")) {
		t.Fatal(string(current), err)
	}
	previous, err := os.Stat(filepath.Join(root, "ArcanumModern", "launcher-ui.previous.log"))
	if err != nil || previous.Size() != processLogLimit {
		t.Fatal(previous, err)
	}
	blocked := t.TempDir()
	if err := os.WriteFile(filepath.Join(blocked, "ArcanumModern"), []byte("blocked"), 0600); err != nil {
		t.Fatal(err)
	}
	l = newProcessLog(blocked, "launcher-ui.log")
	defer l.Close()
	if n, err := l.Write(data); n != len(data) || err != nil {
		t.Fatal(n, err)
	}
}
