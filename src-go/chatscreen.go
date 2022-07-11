package main

import "net/http"

func getchatscreen(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" {
		//handle post request
	} else if r.Method == "GET" {
		if r.URL.Path == "/chatscreen/js/modalwindow.js" ||
			r.URL.Path == "/chatscreen/js/chatlogic.js" ||
			r.URL.Path == "/chatscreen/js/display_onload.js" ||
			r.URL.Path == "/chatscreen/css/chat_style.css" ||
			r.URL.Path == "/chatscreen/css/dropdownlist.css" ||
			r.URL.Path == "/chatscreen/css/modalwindow.css" {
			http.ServeFile(w, r, r.URL.Path[1:])
			return
		}

		http.ServeFile(w, r, "chatscreen/chatscreen.html")
		return
	}
	http.Error(w, "Method is not supported.", http.StatusNotFound)
	return
}
