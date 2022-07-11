const INITIAL_USER_ID = 0;
const INITIAL_MESSAGE_ID = -1;
const INITIAL_CHATROOM_ID = -1;

const ON_SCROLL = 1;
const OFF_SCROLL_REFRESH = 0;

const ROOMCHAT_SELECTED = 0;
const PERSONALCHAT_SELECTED = 1;

let chatroom = 0;
let userID = INITIAL_USER_ID;

let chat = document.getElementById("chat");
let msg = document.getElementById("message");
let oldestMessageID;
let firstTimeRead = false;

let searchbox = document.getElementById("searchBox");
searchbox.addEventListener("input", search);

let xmlhttprefreshChat= new XMLHttpRequest();

//websocket
var websocket = new WebSocket("wss://www.codeatyolo.link/fakelink");
websocket.onopen = () => {
	chooseChatroom_first_time();
}
//after receiving message
websocket.onmessage = function(event){
	let allowWriting = true;
	data = JSON.parse(event.data);
	console.log(data);
	if(chatroom != INITIAL_CHATROOM_ID && chatroom != data["room"]){
		console.log(data["room"]);
		return;
	}
 	else if(userID != INITIAL_USER_ID){
		console.log("private chat received");
		tempUserID = userID.substring(4, userID.length);
		if(tempUserID == data["sender_id"] ||  myID == data["sender_id"]){
		 	console.log("userid: " + tempUserID);
		 	console.log("myID:" + myID);
		 	//allow writing
		 }
		else{
			console.log("sent in different room");
			console.log("userid: " + tempUserID);
                        console.log("myID:" + myID);
			return;
		}
	}
	if(allowWriting){
		id = data["sender_id"];
		let img = document.createElement("img");
		img.className = "profile-chat";
		img.src = "../common/fetchIcon.php?id=" + id;
		img.onerror = function(){
			//this.src = "../photo.jpg";
                        this.src = "/defaultIcon.png";
		}
		let div = document.createElement("tr");
		div.className = "chat-whole";
		
		let liTag_message = document.createElement("li");
		liTag_message.className = "chat-message";
		liTag_message.textContent = data["message"];
		let liTag_datename = document.createElement("li");
		liTag_datename.className = "chat-datename";
		liTag_datename.textContent = data["datename"];
		div.appendChild(liTag_datename);
		div.appendChild(liTag_message);

		if(chat.scrollTop == (chat.scrollHeight - chat.clientHeight)){
			console.log("here");
			firstTimeRead = true;
		}
		chat.appendChild(img);
		chat.appendChild(div);
		chat.appendChild(document.createElement("br"));
		scrollToBottom();
	}
}

chat.onscroll = function(){
	if(chat.scrollTop == 0 && firstTimeRead != true){
		if(chatroom == INITIAL_CHATROOM_ID && userID  == INITIAL_USER_ID){
			return;
		}
		if(oldestMessageID !== ""){
			fetchChat(1);
		}
	}
}

msg.addEventListener("keypress", function send(e){
	if(e.key ==="Enter"){
		sendMessage();
	}
	else{
		//do nothing
	}
});
function fetchChat(isScroll){
	console.log("Scrolling: " + isScroll);
	if(chatroom == INITIAL_CHATROOM_ID && userID  == INITIAL_USER_ID){
		return;
	}
	xmlhttprefreshChat.abort();
	xmlhttprefreshChat.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			console.log(this.responseText);
			let obj = JSON.parse(this.responseText);
			appendChat(obj, isScroll);
			scrollToBottom();
		}
	};
	let tempUserID;
	if(userID != INITIAL_USER_ID){
	        tempUserID= userID.substring(4, userID.length);
        }
	else if(chatroom != INITIAL_CHATROOM_ID){
		tempUserID = INITIAL_USER_ID;
	}
	let msgID;
	if(isScroll == 1){
		msgID = oldestMessageID;
	}
	else if(isScroll == 0){
		//msgID = latestMessageID;
		msgID = INITIAL_MESSAGE_ID;
	}
	let getURL= "loadChat.php?chatroom="+chatroom+"&userID="+tempUserID+"&msgID="+msgID+"&isScroll="+isScroll;
	xmlhttprefreshChat.open("GET", getURL, true);
	xmlhttprefreshChat.send();
}

function filterMessage(message){
	if(message.length < 1){
		return [false, ""];
	}
	let len = message.replace(/\s/g, '').length;
	if(!len){
		return [false, ""];
	}
	len = message.replace(/\p{blank}/g, '').length;
	if(!len){
		return [false, ""];
	}
	message = message.replace(/\\/g, "\\\\");
	message = message.replace(/\"/g, "\\\"");
	return [true, message];
}
function sendMessage(){
	if(chatroom == INITIAL_CHATROOM_ID && userID  == INITIAL_USER_ID){
		return;
	}
	let message = msg.value;
	msg.value = "";
	let flag_and_msgValue = filterMessage(message);
	if(flag_and_msgValue[0] == false){
		return;
	}
	message = flag_and_msgValue[1];
	let tempUserID;
	if(userID != INITIAL_USER_ID){
		tempUserID= userID.substring(4, userID.length);
	}
	else if(chatroom != INITIAL_CHATROOM_ID){
		tempUserID = INITIAL_USER_ID;
	}
	let postData = JSON.stringify({connect:false, sender_id: myID, 
		chatroom:chatroom, message:message, name: fullname, 
	    user_id:tempUserID})
	console.log(postData);
	websocket.send(postData);
}
function chooseChatroom(e, judge){
	let node;
	reverseColor();
	if(judge === ROOMCHAT_SELECTED){
		//親の要素がIDを持っているため、親要素を使う
		node = e.target.parentElement;
		userID = INITIAL_USER_ID;
		chatroom = Number(node.id);
	}
	else if(judge === PERSONALCHAT_SELECTED){
		node = e.target;
		chatroom = INITIAL_CHATROOM_ID;
		userID = node.id;
		console.log("userID: " + userID)
	}
	node.className = "grey";
	chat.innerHTML = "";
	message.value = "";
	//初期表示フラグを立てる。
	firstTimeRead = true;
	//最後のメッセージIDを初期化する。
	//latestMessageID = INITIAL_MESSAGE_ID;
	oldestMessageID = INITIAL_MESSAGE_ID;
	//スコロールのAJAXが送信されていれば、それをキャンセル;
	websocket.send("{\"connect\":true, \"user_id\":\""+myID+"\"}");
	fetchChat(0);
}
function go_to_my_profile(){
	window.location.href = "/edit";
}
function chooseChatroom_first_time(){
	let node;
	node = document.getElementById("0");
	if(node != null){
		console.log("hrer");
	        console.log(node);
	        node.className = "grey";
	        chat.innerHTML = "";
	        message.value = "";
	        //初期表示フラグを立てる。
	        firstTimeRead = true;
	        //最後のメッセージIDを初期化する。
	        oldestMessageID = INITIAL_MESSAGE_ID;
	        //スコロールのAJAXが送信されていれば、それをキャンセル;
	        websocket.send("{\"connect\":true, \"user_id\":\""+myID+"\"}");
        	fetchChat(0);
        }
}
function appendChat(obj, scroll){
	if(obj["NoRoom"] === true){
		window.location.href = "../common/error.php?type=3";
	}
	if(obj["SessionOut"] === true){
		window.location.href = "../common/error.php?type=1";
	}
	if(obj["msg"] == undefined){
		return;
	}
	if(scroll == OFF_SCROLL_REFRESH){
		//latestMessageID = Number(obj["msgID"]);
		if(firstTimeRead == true){
			oldestMessageID = Number(obj["oldmsgID"]);
		}
	}
	else if(scroll == ON_SCROLL){
		oldestMessageID = Number(obj["msgID"]);
	}
	console.log(oldestMessageID);
	let id, msg, date_name;
	msgArray = obj["msg"];
	for(let i = 0; i<msgArray.length; i+=3){
		id = msgArray[i];
		date_name = msgArray[i+1];
		msg = msgArray[i+2];
		let img = document.createElement("img");
		img.className = "profile-chat";
		img.src = "../common/fetchIcon.php?id=" + id;
		img.onerror = function(){
			this.src = "/defaultIcon.png";
		}

		let div = document.createElement("tr");
		div.className = "chat-whole";
		
		let liTag_message = document.createElement("li");
		liTag_message.className = "chat-message";
		liTag_message.textContent = msg;
		let liTag_datename = document.createElement("li");
		liTag_datename.className = "chat-datename";
		liTag_datename.textContent = date_name;
		div.appendChild(liTag_datename);
		div.appendChild(liTag_message);
		if(scroll == ON_SCROLL){
			chat.prepend(document.createElement("br"));
			chat.prepend(div);
			chat.prepend(img);
		}
		else if(scroll == OFF_SCROLL_REFRESH){
			chat.appendChild(img);
			chat.appendChild(div);
			//改行
			chat.appendChild(document.createElement("br"));
		}	
	}
	scrollToBottom();
}
function leaveRoom(e){
	xmlhttprefreshChat.abort();
	reverseColor();
	let targetNode = e.target.parentElement.parentElement.parentElement;
	chatroom = Number(targetNode.id);
	targetNode.className = "grey";

	if(confirm("do you want to abandon this room？")){
		console.log("here");
		let leaveRoomForm = document.createElement("form");
		leaveRoomForm.method = "GET";
		leaveRoomForm.action = "leaveRoom.php";
		let tempRoomID = document.createElement("input");
		tempRoomID.setAttribute("type","hidden");
		tempRoomID.setAttribute("name","roomId");
		tempRoomID.setAttribute("value", chatroom);
		leaveRoomForm.appendChild(tempRoomID);
		document.body.appendChild(leaveRoomForm);
		leaveRoomForm.submit();
	}
	// return;
}
function reverseColor(){
	if(chatroom != INITIAL_CHATROOM_ID){
		document.getElementById(chatroom).className = "normal";
	}
	if(userID != INITIAL_USER_ID){
		document.getElementById(userID).className = "normal";
	}
}
//**********************************検索欄****************************
//個人チャットリスト   と   グループチャットリスト
let privateCollection = document.getElementById("PrivateListBox").getElementsByTagName("div");
let groupCollection = document.getElementById("groupListBox").getElementsByTagName("div");
function search(){
	for(let i = 0; i < privateCollection.length; i++){
		if(!(privateCollection[i].innerHTML).includes(searchbox.value)){
			privateCollection[i].style.display = "none";
		}
		else{
			privateCollection[i].style.display = "block";
		}
	}
	for(let i = 0; i < groupCollection.length; i++){
		let dummy = groupCollection[i].getElementsByTagName("div")[0];
		if(dummy != undefined){
			if(!(dummy.innerHTML).includes(searchbox.value) && dummy != undefined){
				groupCollection[i].style.display = "none";
			}
			else{
				groupCollection[i].style.display = "block";
			}
		}
	}
}
function scrollToBottom(){
	if(firstTimeRead){
		chat.scrollTop = chat.scrollHeight - chat.clientHeight;
		firstTimeRead = false;
	}
}
