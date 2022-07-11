var myID;
var fullname;

get_my_info();

function generate_table(){
	

}
function get_my_info(){
	let xmlrequest = new XMLHttpRequest();
	xmlrequest.onreadystatechange = () => {
		if (xmlrequest.readyState == 4 && xmlrequest.status == 200) {
			console.log(xmlrequest.responseText);
			json = JSON.parse(xmlrequest.responseText);
			myID = json["id"];
			fullname = json["lastname"] + " " + json["firstname"];
			document.getElementById("lastname").value = json["lastname"];
			document.getElementById("firstname").value = json["firstname"];
			document.getElementById("user_id").value =json["user_id"];
			let header = document.getElementById("header");
			let table = document.createElement("table");
			let th = document.createElement("th");
			let td1 = document.createElement("td");
			let td2 = document.createElement("td");
			td1.innerHTML = fullname;
			td2.innerHTML = "<a href = '/logout'>Sign out</a>";
			th.appendChild(td1);
			th.appendChild(td2);
			table.appendChild(th);
			header.appendChild(table);
		}
	}
	xmlrequest.open("GET", "../common/getInfo.php");
	xmlrequest.send();
}

async function setProfilePicture(){
	let proPic = document.getElementById("profileimage");
	let blob;
	let reader = new window.FileReader();
	let xmlhttpicon = new XMLHttpRequest();
	xmlhttpicon.responseType = "blob"; //byteArray + MIME

	xmlhttpicon.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			blob = this.response;
			//base64のURL
			reader.readAsDataURL(blob);
			reader.onload = function(){
				proPic.src = reader.result;
				// proPic.src = "/defaultIcon.png";
			};
			reader.onerror = function (){
				proPic.src = "/defaultIcon.png";
			}
		}
		else{
			proPic.src = "/defaultIcon.png";
		}
	};
	xmlhttpicon.open("GET", "../common/fetchIcon.php", true);
	xmlhttpicon.send();
}

function generate_two_buttons(){
	let div_addAndExit = document.createElement("div");
	div_addAndExit.className = "addAndExit";
	let div_colon = document.createElement("div");
	div_colon.innerHTML = ":";
	let ul = document.createElement("ul");
	ul.className = "addAndExitOption";
	let li1 = document.createElement("li");
	li1.className = "option";
	li1.innerHTML = "Invite member";
	li1.setAttribute("onclick", "showModalWindow(event)");
	let li2 = document.createElement("li");
	li2.className = "option";
	li2.innerHTML = "Leave room";
	li2.setAttribute("onclick", "leaveRoom(event)");
	div_addAndExit.appendChild(div_colon);
	ul.appendChild(li1);
	ul.appendChild(li2);
	div_addAndExit.appendChild(ul);
	return div_addAndExit;
}

function inject_grouproom(ary){
	let groupListBox = document.getElementById("groupListBox");
	console.log(ary);
	console.log(groupListBox);
	for(i = 0; i < ary.length; i++){
		let div_id = document.createElement("div");
		div_id.setAttribute("id", ary[i]);
		if (i == 0){
			let div_onclick = document.createElement("div");
			div_onclick.setAttribute("onclick", "chooseChatroom(event, 0)");
			div_onclick.innerHTML = "Lobby(Everyone)";
			div_id.appendChild(div_onclick);
		}
		else{
			let div_onclick = document.createElement("div");
			div_onclick.setAttribute("onclick", "chooseChatroom(event, 0)");
			div_onclick.innerHTML = "Chatroom " + ary[i];
			div_id.appendChild(div_onclick);
			let two_buttons = generate_two_buttons();
			div_id.appendChild(two_buttons);
		}
		groupListBox.appendChild(div_id);
	}
}
function inject_privateroom(ary){
	let privateListBox = document.getElementById("PrivateListBox");
	for(i = 0; i<ary.length; i=i+2){
		let div = document.createElement("div");
		div.setAttribute("id", ary[i+1]);
		div.setAttribute("onclick", "chooseChatroom(event, 1)");
		div.style.display = "block";
		div.innerHTML = ary[i];
		privateListBox.appendChild(div);
	}
}

function fetch_my_rooms(){
	let xmlhttpicon = new XMLHttpRequest();

	xmlhttpicon.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			console.log(this.responseText);
			json = JSON.parse(this.responseText);
			console.log(json["grouproom"]);
			console.log(json["privateroom"]);
			inject_grouproom(json["grouproom"]);
			inject_privateroom(json["privateroom"]);
		}
	};
	xmlhttpicon.open("GET", "../chat/fetchRooms.php", true);
	xmlhttpicon.send();
}