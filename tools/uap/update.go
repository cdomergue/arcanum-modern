package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"
)

const latestReleaseAPI = "https://api.github.com/repos/cdomergue/arcanum-modern/releases/latest"

// Releases use stable major.minor.patch tags; drafts and prereleases are never offered.
func releaseVersion(tag string) ([3]uint64, bool) {
	var version [3]uint64
	if len(tag) > 64 {
		return version, false
	}
	parts := strings.Split(strings.TrimPrefix(tag, "v"), ".")
	if len(parts) != 3 {
		return version, false
	}
	for i, part := range parts {
		if part == "" || (len(part) > 1 && part[0] == '0') {
			return version, false
		}
		for _, c := range part {
			if c < '0' || c > '9' {
				return version, false
			}
		}
		n, err := strconv.ParseUint(part, 10, 64)
		if err != nil {
			return version, false
		}
		version[i] = n
	}
	return version, true
}

func newerRelease(current, tag string) bool {
	a, ok := releaseVersion(current)
	b, valid := releaseVersion(tag)
	if !ok || !valid {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return b[i] > a[i]
		}
	}
	return false
}

func latestUpdate(client *http.Client, endpoint, current string) string {
	if _, ok := releaseVersion(current); !ok {
		return ""
	}
	req, err := http.NewRequest(http.MethodGet, endpoint, nil)
	if err != nil {
		return ""
	}
	req.Header.Set("Accept", "application/vnd.github+json")
	req.Header.Set("User-Agent", "Arcanum-Renaissance/"+current)
	response, err := client.Do(req)
	if err != nil {
		return ""
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return ""
	}
	const limit = 1 << 20
	raw, err := io.ReadAll(io.LimitReader(response.Body, limit+1))
	if err != nil || len(raw) > limit {
		return ""
	}
	var release struct {
		Tag        string `json:"tag_name"`
		Draft      bool   `json:"draft"`
		Prerelease bool   `json:"prerelease"`
		Published  string `json:"published_at"`
	}
	if json.Unmarshal(raw, &release) != nil || release.Draft || release.Prerelease || release.Published == "" {
		return ""
	}
	if !newerRelease(current, release.Tag) {
		return ""
	}
	return release.Tag
}

func checkUpdate(current string) int {
	client := &http.Client{Timeout: 4 * time.Second,
		CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse }}
	if tag := latestUpdate(client, latestReleaseAPI, current); tag != "" {
		fmt.Println(tag)
	}
	return 0 // An unavailable update service must never prevent playing.
}
