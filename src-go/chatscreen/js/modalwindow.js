//ユーザ固有IDとユーザIDを格納しているフルリスト
let FullList;
//検索結果として画面に表示されるユーザIDを格納する配列
let nameList = [];
//検索対象者のユーザIＤを入力する欄
let memberId = document.getElementById("memberId");
//モーダルウィンドウ
var modal = document.getElementById("myModal");
//検索結果に当てはめるユーザIDが表示されるフィールド
let selectOptions_nodeList = document.getElementById("selectOptions").childNodes;


//「招待」ボタンをクリック時、モーダルウィンドウが表示される 
function showModalWindow(e) {
	modal.style.display = "block";
	memberId.value = "";
	FetchData(e);
}
// モーダルウィンドウの外に押すと、モーダルウィンドウを非表示する。
window.onclick = function(event) {
  if (event.target == modal) {
	//検索結果を削除してから非表示する
	clearSearchResults();
    modal.style.display = "none";
  }
}

//**************************************************入力時に検索内容を該当する結果を表示する*********************************************************
memberId.oninput = function(){
	//スペース、全角スペース、タブスペースを削除する。
	memberId.value = memberId.value.replace(/ /g, "");
	memberId.value = memberId.value.replace(/　/g, "");
	memberId.value = memberId.value.replace(/	/g, "");
	//最後の入力を抽出する。
	let input = memberId.value.split(",");
	input = input[input.length-1];
	let i;
	//pタグの一番上のタグに位置設定する。
	//空白のselectedOptionsの一番上のインデックス
	let emptytag = 1;
	//検索結果をクリアする
	clearSearchResults();
	//検索内容に該当する結果を表示する
	
	if(input !== ""){
		for(i = 0; i < nameList.length; i++){
			if(nameList[i].includes(input)){
				//pタグが全部埋まっていない場合
				if(emptytag < 10){
					selectOptions_nodeList[emptytag].innerHTML = nameList[i];
					emptytag += 2;
				}
				else{
					i = nameList.length;
				}
			}
		}
	}
}
//**************************************************データベースからユーザIDのデータを抽出する******************************
function FetchData(e){
	//ネームリストを格納するための配列
	nameList = [];
	//色を戻す
	reverseColor();
	//選択されたチャットルームのIDを決める
	let targetNode = e.target.parentElement.parentElement.parentElement;
	chatroom = Number(targetNode.id);
	//色を染める。
	targetNode.className = "grey";
	//名前のリストを配列に格納する。
	let xmlhttpget = new XMLHttpRequest();
	let rspns;
	xmlhttpget.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			//レスポンス
			rspns = this.responseText;
			//ユーザ固有IDとユーザIDを格納しているフルリスト
			FullList = rspns.split(",");
			//検索結果として画面に表示されるユーザIDを格納する配列を作る。
			for(let i = 0; i < FullList.length; i+=2){
				nameList.push(FullList[i]);
			}
		}
	};
	xmlhttpget.open("GET", "../chat/fetchNames.php?roomID=" + chatroom, true);
	xmlhttpget.send();
}
//********************************************検索結果の中から選択するとそのユーザIDが入力欄に表示される*********************
function AddToTextBox(e){
	//検索欄に入力した未完成の入力を削除する
	while(memberId.value.slice(-1) !== ',' && memberId.value.length > 0){
		memberId.value = memberId.value.substring(0, memberId.value.length - 1);
	}
	//そして、フルユーザIDを検索欄に付き加える。
	memberId.value += e.target.innerHTML + ","; 
	//キャレットの位置設定
	memberId.focus();
	memberId.setSelectionRange(memberId.value.length, memberId.value.length);
	clearSearchResults();
}
//********************************************招待する人の名前を認証してからDBに挿入する********************************************
function AddMember(){
	let arrayOfNames = StringToArray(memberId.value);
	//ASPファイルに送る文字列
	let postData = "";
	for(let i = 0; i < arrayOfNames.length; i++){
		postData = postData + FullList[(FullList.indexOf(arrayOfNames[i]) + 1)] + ",";
	}
	let xmlhttppost = new XMLHttpRequest();
        xmlhttppost.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
				//モーダルウィンドウを非表示する
				modal.style.display = "none";
            }
        };
	xmlhttppost.open("GET", "addMember.php?members=" + postData + "&roomID=" + chatroom, true);
	xmlhttppost.send();
}
//********************************************入力内容を確認し、正しく入力された人のユーザIDのみを格納する。********************************************
function StringToArray(memberIDs){
	let returnArray = [];
	let splt;
	splt = memberIDs.split(",");
	let i;
	for(i = 0; i < splt.length; i++){
		if(splt[i] !== "" && nameList.indexOf(splt[i]) !== -1){
			returnArray.push(splt[i]);
		}
	}
	return returnArray;
}
//検索結果を空白にする。
function clearSearchResults(){
	for(i = 0; i < selectOptions_nodeList.length; i++){
		selectOptions_nodeList[i].innerHTML = "";
	}
}