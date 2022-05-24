package main

import "net/http"

func getchatscreen(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" || r.Method == "GET" {
		getchatscreen(w, r)
	}

	http.Error(w, "Method is not supported.", http.StatusNotFound)
	return
}
