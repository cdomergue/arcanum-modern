package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

const processLogLimit = 1024 * 1024

// Diagnostics must never prevent the game from starting or writing output.
// Keep the last two bounded segments, flushing each write to the OS.
type processLog struct {
	mu   sync.Mutex
	path string
	file *os.File
	size int64
}

func newProcessLog(root, name string) *processLog {
	l := &processLog{path: filepath.Join(root, "ArcanumModern", name)}
	if os.MkdirAll(filepath.Dir(l.path), 0700) == nil {
		l.file, _ = os.OpenFile(l.path, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
		if l.file != nil {
			if info, err := l.file.Stat(); err == nil {
				l.size = info.Size()
			}
		}
	}
	return l
}

func (l *processLog) Write(p []byte) (int, error) {
	l.mu.Lock()
	defer l.mu.Unlock()
	n := len(p)
	for len(p) > 0 && l.file != nil {
		if l.size >= processLogLimit {
			l.file.Close()
			l.file = nil
			previous := strings.TrimSuffix(l.path, ".log") + ".previous.log"
			if err := os.Remove(previous); err != nil && !os.IsNotExist(err) {
				break
			}
			if os.Rename(l.path, previous) != nil {
				break
			}
			l.file, _ = os.OpenFile(l.path, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
			l.size = 0
			if l.file == nil {
				break
			}
		}
		count := len(p)
		if int64(count) > processLogLimit-l.size {
			count = int(processLogLimit - l.size)
		}
		written, err := l.file.Write(p[:count])
		l.size += int64(written)
		if err != nil || written == 0 {
			break
		}
		p = p[written:]
	}
	return n, nil
}

func (l *processLog) event(event, detail string) {
	fmt.Fprintf(l, "\n%s process_%s %s\n", time.Now().Format(time.RFC3339Nano), event, detail)
}

func (l *processLog) Close() {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.file != nil {
		l.file.Close()
		l.file = nil
	}
}
