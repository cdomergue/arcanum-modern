package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestOfficialDownload(t *testing.T) {
	directory := os.Getenv("UAP_TEST_DOWNLOAD")
	if directory == "" {
		t.Skip("set UAP_TEST_DOWNLOAD for the official download integration check")
	}
	if err := os.MkdirAll(directory, 0700); err != nil {
		t.Fatal(err)
	}
	if err := downloadInstaller(filepath.Join(directory, "UAP2.0.2.exe")); err != nil {
		t.Fatal(err)
	}
}
