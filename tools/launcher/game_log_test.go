package main

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"
)

func TestGameLogRotationAndFailure(t *testing.T) {
	root := t.TempDir()
	l := newGameLog(root)
	data := bytes.Repeat([]byte("x"), gameLogLimit*3+17)
	if n, err := l.Write(data); n != len(data) || err != nil {
		t.Fatal(n, err)
	}
	l.event("exit", "code=0xffffffff")
	l.Close()
	current, err := os.ReadFile(filepath.Join(root, "ArcanumModern", "game-process.log"))
	if err != nil || !bytes.Contains(current, []byte("process_exit code=0xffffffff")) {
		t.Fatal(string(current), err)
	}
	previous, err := os.Stat(filepath.Join(root, "ArcanumModern", "game-process.previous.log"))
	if err != nil || previous.Size() != gameLogLimit {
		t.Fatal(previous, err)
	}
	blocked := t.TempDir()
	if err := os.WriteFile(filepath.Join(blocked, "ArcanumModern"), []byte("blocked"), 0600); err != nil {
		t.Fatal(err)
	}
	l = newGameLog(blocked)
	defer l.Close()
	if n, err := l.Write(data); n != len(data) || err != nil {
		t.Fatal(n, err)
	}
}
