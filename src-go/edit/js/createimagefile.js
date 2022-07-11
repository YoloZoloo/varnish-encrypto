//会員登録画面、会員情報変更画面で、プロフィール画像がアップロードされた際の処理に関するソース

const img = document.getElementById("result");
const iconset = document.getElementById("iconset");
const canvas = document.getElementById('canvas');
const form = document.forms[0];

iconset.addEventListener('change', loadImage);
img.addEventListener('load', cropImage);
canvas.style.display = "none";

//input type="file" チェンジイベント
async function loadImage(){
    //initialize呼び出し
    // initialize();
    let myFile = iconset.files[0];
    img.src = URL.createObjectURL(myFile);

    console.log("URL "+img.src);

    let proPic = document.getElementById("profileimage");
    let canvas = document.getElementById("canvas");
    proPic.style.display = "none";
    canvas.style.display = "block";
    //canvas.style.margin  = "auto";
}

//img(id=result)ロードイベント
function cropImage(){

    const afterwidth = 150;
    const afterheight = 150;

    let ctx = canvas.getContext('2d');
    canvas.width = afterwidth;
    canvas.height = afterheight;
    if(this.height>this.width){
        ctx.drawImage(img,
            0,(this.height/2)-(this.width/2),this.width,this.width,
            0, 0, afterwidth, afterheight
        );
    }
    else{
        ctx.drawImage(img,
            (this.width/2)-(this.height/2),0,this.height,this.height,
            0, 0, afterwidth, afterheight
        );
    }

    //canvas.style.borderRadius = "50%";
    //dataURItoBlobを呼び出し
    let myBlob = dataURItoBlob(canvas.toDataURL("image/png", 0.1));
    //resizeを呼び出し
    resize(myBlob);
}

function dataURItoBlob(dataURI) {
    // convert base64/URLEncoded data component to raw binary data held in a string
    var byteString;
    if (dataURI.split(',')[0].indexOf('base64') >= 0)
        byteString = atob(dataURI.split(',')[1]);
    else
        byteString = unescape(dataURI.split(',')[1]);

    // separate out the mime component
    var mimeString = dataURI.split(',')[0].split(':')[1].split(';')[0];

    // write the bytes of the string to a typed array
    var ia = new Uint8Array(byteString.length);
    for (var i = 0; i < byteString.length; i++) {
        ia[i] = byteString.charCodeAt(i);
    }
    return new Blob([ia], {type:mimeString});
}


function resize(myBlob){
    if(document.forms[0]["images"] !=null){
        let foto = document.getElementById("images");
        document.forms[0].removeChild(foto);
    }
    //resizing
    
    console.log(myBlob);

    //preapare file type input inside form
    let newinput = document.createElement("input");
    newinput.setAttribute("type", "file");
    newinput.setAttribute("name", "images");
    newinput.setAttribute("id", "images");
    newinput.setAttribute('style', 'display:none');
    form.appendChild(newinput);

    //change blob to file type
    let file = new File([myBlob], ".jpg",{type:"image/jpeg", lastModified:new Date().getTime()});
    //prepare container object and use it to add file to form
    let container = new DataTransfer();
    container.items.add(file);
    newinput.files = container.files;

    console.log(document.forms[0]["images"]);
}
