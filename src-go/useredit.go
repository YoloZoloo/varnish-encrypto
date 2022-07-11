package main

import "net/http"

func geteditscreen(w http.ResponseWriter, r *http.Request) {
	// http.ServeFile(w, r, "edit/edit.html")
	if r.Method == "POST" {
		//handle post request
	} else if r.Method == "GET" {
		if r.URL.Path == "/edit/js/createimagefile.js" ||
			r.URL.Path == "/edit/js/display_onload.js" ||
			r.URL.Path == "/edit/js/entry-edit.js" ||
			r.URL.Path == "/edit/css/entry-edit.css" ||
			r.URL.Path == "/edit/css/memberslist-style.css" ||
			r.URL.Path == "/edit/entry.html" {
			http.ServeFile(w, r, r.URL.Path[1:])
			return
		}

		http.ServeFile(w, r, "edit/edit.html")
		return
	}
	http.Error(w, "Method is not supported.", http.StatusNotFound)
	return
}
