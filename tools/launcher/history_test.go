package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestHistoryPaginationAndStableFiltering(t *testing.T) {
	calls := 0
	s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		calls++
		if r.URL.Query().Get("page") != fmt.Sprint(calls) || r.URL.Query().Get("per_page") != "30" {
			t.Error("incorrect pagination")
		}
		var batch []release
		if calls == 1 {
			for i := 0; i < 30; i++ {
				batch = append(batch, release{Tag: "v9.0.0", Published: "today", Prerelease: true})
			}
		} else {
			for i := 6; i >= 0; i-- {
				batch = append(batch, release{Tag: fmt.Sprintf("v2.0.%d", i), Published: "today", Body: "## Changes", Assets: []asset{{Name: "ignored"}}})
			}
			batch = append(batch, release{Tag: "v2.0.6", Published: "today"}, release{Tag: "v8.0.0", Draft: true}, release{Tag: "invalid", Published: "today"})
		}
		_ = json.NewEncoder(w).Encode(batch)
	}))
	defer s.Close()
	history, err := releaseHistory(context.Background(), s.Client(), s.URL)
	if err != nil || calls != 2 || len(history) != 5 {
		t.Fatalf("history=%v calls=%d err=%v", history, calls, err)
	}
	if history[0].Tag != "v2.0.6" || history[4].Tag != "v2.0.2" || len(history[0].Assets) != 0 {
		t.Fatal(history)
	}
	text := historyText(history)
	if strings.Count(text, "Changes") != 5 || strings.Contains(text, "##") {
		t.Fatal(text)
	}
}

func TestHistoryFailureAndLatestMerge(t *testing.T) {
	s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { http.Error(w, "unavailable", 503) }))
	defer s.Close()
	if _, err := releaseHistory(context.Background(), s.Client(), s.URL); err == nil {
		t.Fatal("expected error")
	}
	cached := []release{{Tag: "v2.2.0", Published: "today", Body: "old"}, {Tag: "v2.1.5", Published: "yesterday"}}
	fresh := release{Tag: "v2.2.0", Published: "today", Body: "revised"}
	merged := recentReleases(append([]release{fresh}, cached...))
	if len(merged) != 2 || merged[0].Body != "revised" {
		t.Fatal(merged)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := releaseHistory(ctx, s.Client(), s.URL); err == nil {
		t.Fatal("ignored cancellation")
	}
}
