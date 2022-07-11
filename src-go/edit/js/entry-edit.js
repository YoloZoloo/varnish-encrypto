//会員登録画面、会員情報変更画面のJSファイル

//「登録」「変更」ボタン押下時の処理
document.getElementById('submitButton').addEventListener('click',function(){

    //「名」「ユーザID」「パスワード」「パスワード確認」の4つの項目の入力が十分であるかを示すフラグ
    let isInputComplete = true;

    //「名」チェック
    if(document.getElementById('firstname').value == ""){
        document.getElementById('nameAlert').innerHTML="Enter your firstname";
        isInputComplete = false;
    }else{
        document.getElementById('nameAlert').innerHTML="";
    }

    //「ユーザID」チェック
    if(document.getElementById('user_id').value == ""){
        document.getElementById('user_idAlert').innerHTML="Enter user ID";
        isInputComplete = false;
    }else if(document.getElementById('user_id').value.match(/[0-9a-zA-Z]/g).length != document.getElementById('user_id').value.length){
        document.getElementById('user_idAlert').innerHTML="Only half width number and characters can be used";
        isInputComplete = false;
    }else{
        document.getElementById('user_idAlert').innerHTML="";
    }

    //登録画面「パスワード」チェック
    if(document.getElementById("password") != null){
        const passwordObj = document.getElementById('password');
        if(passwordObj.value==""){
            document.getElementById('passwordAlert').innerHTML="Enter password pls";
            isInputComplete = false;
        }
        else if(passwordObj.value.length < 6 || passwordObj.value.length > 18){
            document.getElementById('passwordAlert').innerHTML="Password must be between 6 to 18 characters";
            isInputComplete = false;
        }
        else if(passwordObj.value.search(/[0-9]/g) == -1 || passwordObj.value.search(/[a-z]/g) == -1 || passwordObj.value.search(/[A-Z]/g) == -1){
            document.getElementById('passwordAlert').innerHTML="Password needs to include capital, non-capital characters and number";
            isInputComplete = false;
        }else if(passwordObj.value.match(/[0-9a-zA-Z!"#$%&'()*+,-./:;<=>?@\[\\\]^_`{|}~]/g).length != passwordObj.value.length){
            document.getElementById('passwordAlert').innerHTML="Entered invalid characters";
            isInputComplete = false;
        }else{
            document.getElementById('passwordAlert').innerHTML="";
        }
    }

    //登録画面「パスワード確認入力」チェック
    if(document.getElementById("passwordCheck") != null){
        //「パスワード確認」チェック
        if(document.getElementById('passwordCheck').value != document.getElementById('password').value){
            document.getElementById('passwordCheckAlert').innerHTML="passwords are not matching";
            isInputComplete = false;
        }else{
            document.getElementById('passwordCheckAlert').innerHTML="";
        }
    }

    //編集画面「現在のパスワード」チェック
    if(document.getElementById("beforePassword") != null){
        if(document.getElementById('beforePassword').value==""){
            document.getElementById('beforePasswordAlert').innerHTML="Please enter password";
            isInputComplete = false;
        }else{
            document.getElementById('beforePasswordAlert').innerHTML="";
        }
    }

    
    if(document.getElementById("afterPassword") != null && document.getElementById("afterPasswordCheck") != null){
    //編集画面「変更後のパスワード」「変更後のパスワード確認」チェック(マイアカウント編集)
        const aftrPsObj = document.getElementById("afterPassword");
        const afterPsChkObj = document.getElementById("afterPasswordCheck");
        if(aftrPsObj.value == "" && afterPsChkObj.value == ""){
            //変更後パスワード入力欄・確認欄が空白の場合、処理なし
        }
        else if(aftrPsObj.value.search(/[0-9]/g) == -1 || aftrPsObj.value.search(/[a-z]/g) == -1 || aftrPsObj.value.search(/[A-Z]/g) == -1){
            document.getElementById('afterPasswordAlert').innerHTML="Password needs to include capital, non-capital characters and number";
            isInputComplete = false;
        }else if(aftrPsObj.value.match(/[0-9a-zA-Z!"#$%&'()*+,-./:;<=>?@\[\\\]^_`{|}~]/g).length != aftrPsObj.value.length){
            document.getElementById('afterPasswordAlert').innerHTML="Entered invalid characters";
            isInputComplete = false;
        }else{
            document.getElementById('afterPasswordAlert').innerHTML="";
        }

        if(aftrPsObj.value != afterPsChkObj.value){
            document.getElementById('afterPasswordCheckAlert').innerHTML="passwords are not matching";
            isInputComplete = false;
        }else{
            document.getElementById('afterPasswordCheckAlert').innerHTML="";
        }
    }
    else if(document.getElementById("afterPassword") != null){
    //編集画面「変更後のパスワード」チェック(管理者用)
        const aftrPsObj = document.getElementById("afterPassword");
        if(aftrPsObj.value==""){
            document.getElementById('afterPasswordAlert').innerHTML="Please enter password";
            isInputComplete = false;
        }
        else if(aftrPsObj.value.search(/[0-9]/g) == -1 || aftrPsObj.value.search(/[a-z]/g) == -1 || aftrPsObj.value.search(/[A-Z]/g) == -1){
            document.getElementById('afterPasswordAlert').innerHTML="Password needs to include capital, non-capital characters and number";
            isInputComplete = false;
        }else if(aftrPsObj.value.match(/[0-9a-zA-Z!"#$%&'()*+,-./:;<=>?@\[\\\]^_`{|}~]/g).length != aftrPsObj.value.length){
            document.getElementById('afterPasswordAlert').innerHTML="Entered invalid characters";
            isInputComplete = false;
        }else{
            document.getElementById('afterPasswordAlert').innerHTML="";
        }
    }

    //入力が十分ならば、フォームを送信
    if(isInputComplete == true){
        document.entryEditForm.submit();
    }
    else window.alert('Form is incomplete.');
});

//マイアカウント登録時
if(document.getElementById("passwordCheck") != null){
    //「パスワード確認」フォーカスアウト時イベント
    document.getElementById('passwordCheck').addEventListener('change',function(){
        if(document.getElementById('passwordCheck').value != document.getElementById('password').value){
            //「パスワード」入力欄と一致しない場合
            document.getElementById('passwordCheckAlert').innerHTML="passwords are not matching"
        }else{
            document.getElementById('passwordCheckAlert').innerHTML="";
        }
    });
}

//マイアカウント編集時
if(document.getElementById("afterPasswordCheck") != null){
    //「パスワード確認」フォーカスアウト時イベント
    document.getElementById('afterPasswordCheck').addEventListener('change',function(){
        if(document.getElementById('afterPasswordCheck').value != document.getElementById('afterPassword').value){
            //「パスワード」入力欄と一致しない場合
            document.getElementById('afterPasswordCheckAlert').innerHTML="passwords are not matching"
        }else{
            document.getElementById('afterPasswordCheckAlert').innerHTML="";
        }
    });
}

//「ユーザID」入力時イベント
document.getElementById('user_id').addEventListener('input',function(){
    let str = document.getElementById('user_id').value;
    let strMatch='';
    if(str.match(/[0-9a-zA-Z]*/g)[0] != str){
        document.getElementById('user_idAlert').innerHTML="Only half width number and characters can be used"
    }else{
        document.getElementById('user_idAlert').innerHTML="";
    }

    for(let i =0; i < str.match(/[0-9a-zA-Z]*/g).length; i++){
        strMatch += str.match(/[0-9a-zA-Z]*/g)[i];
    }
    document.getElementById('user_id').value = strMatch;
});

if(document.getElementById("deleteAccountButton") != null){
//アカウント削除ボタンが存在する場合
    document.getElementById('deleteAccountButton').addEventListener('click',function(){
    //アカウント削除ボタン押下時の処理
        let result = window.confirm('Do you want to delete your account?');
        if(result==true){
            document.deleteAccount.submit();
        }
        else{
            return;
        }
    });
}

if(document.getElementById("resignAdminButton") != null){
//管理者辞任ボタンが存在する場合
    document.getElementById('resignAdminButton').addEventListener('click',function(){
    //管理者辞任ボタン押下時の処理
        let result = window.confirm('Do you want to resign admin status?');
        if(result==true){
            document.resignAdmin.submit();
        }
        else{
            return;
        }
    });
}

if(document.getElementById("assignAdminButton") != null){
//管理者権限付与ボタンが存在する場合
    document.getElementById('assignAdminButton').addEventListener('click',function(){
    //管理者権限付与ボタン押下時の処理
        let result = window.confirm('Do you want to give out admin status?');
        if(result==true){
            document.assignAdmin.submit();
        }
        else{
            return;
        }
    });
}

