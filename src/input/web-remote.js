function isoldeRemote(options) {
    "use strict";
    if(globalThis.__isoldeRemote)return globalThis.__isoldeRemote;
    if(!options || !options.focusColor || !options.surfaceColor || !options.textColor)return null;
    const root=document.documentElement;if(!root){addEventListener("DOMContentLoaded",()=>isoldeRemote(options),{once:true});return null;}
    const scale=Number(options.typographyScale)||1,corners=Number(options.cornerRadius)||0;
    let current=null,keyboard=null,pageMenu=null,menuIndex=0,draft="",original="",keyIndex=0,upper=false;
    const owned=new WeakSet();
    function overlayParent(){return current && current.closest('dialog[open]') || root;}
    function box(style){const item=document.createElement("div");owned.add(item);Object.assign(item.style,{fontFamily:"sans-serif",...style});overlayParent().appendChild(item);return item;}
    const ring=box({position:"fixed",pointerEvents:"none",zIndex:"2147483646",boxSizing:"border-box",border:"3px solid "+options.focusColor,borderRadius:corners+"px",display:"none"});
    const hint=box({position:"fixed",pointerEvents:"none",zIndex:"2147483646",bottom:"0",left:"0",right:"0",padding:"12px 20px",background:options.surfaceColor,color:options.textColor,fontSize:(17*scale)+"px",textAlign:"center"});
    function tell(){hint.textContent="D-pad Move highlight · OK Select · Menu Page actions · Home Isolde";}
    tell();
    function visible(el){if(!el || !el.isConnected || owned.has(el) || el.matches(':disabled') || el.getAttribute("aria-disabled")==="true" || el.closest('[inert],[aria-hidden="true"]'))return false;const s=getComputedStyle(el),r=el.getBoundingClientRect();return s.display!=="none" && s.visibility!=="hidden" && Number(s.opacity)!==0 && r.width>1 && r.height>1;}
    function candidates(){
        const dialogs=Array.from(document.querySelectorAll('dialog[open],[role="dialog"][aria-modal="true"]')).filter(visible);
        const items=[],pending=[dialogs.length?dialogs[dialogs.length-1]:document];let scanned=0;
        while(pending.length && scanned<4096){
            const scope=pending.shift(),walk=document.createTreeWalker(scope,NodeFilter.SHOW_ELEMENT);
            for(let el=walk.nextNode();el && scanned++<4096;el=walk.nextNode()){
                if(el.shadowRoot)pending.push(el.shadowRoot);
                if((el.matches('a[href],button,input:not([type="hidden"]),textarea,select,summary,iframe,[role="button"],[role="link"],[role="tab"],[role="menuitem"],[role="checkbox"],[role="radio"],[role="switch"],[contenteditable="true"]') || el.hasAttribute('tabindex') && el.tabIndex>=0) && visible(el) && !el.closest('[data-isolde-remote-keyboard],[data-isolde-remote-menu]'))items.push(el);
                if(items.length>=1024)return items;
            }
        }
        return items;
    }
    function paint(){
        // Browser top-layer dialogs outrank every document z-index.
        const parent=overlayParent();if(ring.parentNode!==parent)parent.appendChild(ring);if(hint.parentNode!==parent)parent.appendChild(hint);
        if(pageMenu){ring.style.display="none";return;}
        if(!visible(current)){ring.style.display="none";return;}
        const r=current.getBoundingClientRect();Object.assign(ring.style,{display:"block",left:(r.left-4)+"px",top:(r.top-4)+"px",width:(r.width+8)+"px",height:(r.height+8)+"px",borderRadius:corners+"px"});
    }
    function select(el){if(!visible(el))return false;current=el;el.focus({preventScroll:true});el.scrollIntoView({block:keyboard?"start":"nearest",inline:"nearest",behavior:"instant"});paint();return true;}
    function move(direction){
        const all=candidates();if(!all.length){scrollBy(0,(direction==="up"?-1:1)*innerHeight*0.6);return;}
        if(!visible(current) || !all.includes(current)){select(all[0]);return;}
        const from=current.getBoundingClientRect(),cx=from.left+from.width/2,cy=from.top+from.height/2;
        let best=null,score=Infinity;
        for(const el of all){if(el===current)continue;const r=el.getBoundingClientRect(),dx=r.left+r.width/2-cx,dy=r.top+r.height/2-cy;
            const primary=direction==="right"?dx:direction==="left"?-dx:direction==="down"?dy:-dy,side=Math.abs(direction==="right" || direction==="left"?dy:dx);
            if(primary<=1)continue;const candidate=primary+side*2.5+(side>primary?side*4:0);if(candidate<score){score=candidate;best=el;}}
        if(best)select(best); // At an edge keep the selected control visible. Menu offers explicit page scrolling.
    }
    function textField(el){return el && (el.isContentEditable || el.tagName==="TEXTAREA" || el.tagName==="INPUT" && !["button","submit","reset","checkbox","radio","range","file","color","hidden"].includes(el.type));}
    function scrollPage(amount){
        let node=current && current.parentElement;
        while(node && node!==root){if(node.scrollHeight>node.clientHeight+1 && /auto|scroll/.test(getComputedStyle(node).overflowY)){node.scrollBy(0,amount*node.clientHeight);paint();return;}node=node.parentElement;}
        scrollBy(0,amount*innerHeight);paint();
    }
    function closeMenu(){if(pageMenu){pageMenu.remove();pageMenu=null;paint();}return true;}
    function menuPaint(){Array.from(pageMenu.children).forEach((button,index)=>button.style.outline=index===menuIndex?"3px solid "+options.focusColor:"none");}
    function menuSelect(){const index=menuIndex;closeMenu();if(index===1)scrollPage(-0.6);else if(index===2)scrollPage(0.6);else if(index===3 && history.length>1)history.back();}
    function openMenu(){
        if(pageMenu)return closeMenu();menuIndex=0;
        pageMenu=box({position:"fixed",zIndex:"2147483647",top:"20%",left:"30%",right:"30%",padding:"20px",background:options.surfaceColor,color:options.textColor,border:"3px solid "+options.focusColor,borderRadius:corners+"px"});
        pageMenu.setAttribute("data-isolde-remote-menu","");pageMenu.setAttribute("role","dialog");pageMenu.setAttribute("aria-label","Page actions");
        ["Return to page","Scroll up","Scroll down","Back a page"].forEach((label,index)=>{const button=document.createElement("button");button.type="button";button.textContent=label;Object.assign(button.style,{display:"block",width:"100%",padding:"16px",marginBottom:"10px",fontSize:(22*scale)+"px",color:options.textColor,background:options.surfaceColor,border:"2px solid "+options.focusColor,borderRadius:corners+"px"});button.addEventListener("click",()=>{menuIndex=index;menuSelect();});pageMenu.appendChild(button);});
        menuPaint();paint();return true;
    }
    function closeKeyboard(apply){
        if(!keyboard)return false;
        const input=current;keyboard.remove();keyboard=null;
        if(apply && input && input.isConnected){
            if(input.isContentEditable)input.textContent=draft;
            else{const proto=input.tagName==="TEXTAREA"?HTMLTextAreaElement.prototype:HTMLInputElement.prototype;Object.getOwnPropertyDescriptor(proto,"value").set.call(input,draft);}
            input.dispatchEvent(new Event("input",{bubbles:true}));input.dispatchEvent(new Event("change",{bubbles:true}));
        }
        draft="";original="";paint();return true;
    }
    const rows=["1234567890","qwertyuiop","asdfghjkl@","zxcvbnm.-_",["Shift","Space","Delete","Cancel","Done"]];
    let keys=[];
    function renderKeyboard(){
        keyboard.replaceChildren();
        const caption=document.createElement("div");caption.textContent=current.type==="password"?"•".repeat(Math.min(80,draft.length)):draft;Object.assign(caption.style,{minHeight:"1.4em",padding:"6px",overflow:"hidden",whiteSpace:"nowrap"});keyboard.appendChild(caption);
        keys=[];
        for(let row=0;row<rows.length;row++){
            const line=document.createElement("div");Object.assign(line.style,{display:"flex",gap:"6px",marginTop:"6px"});keyboard.appendChild(line);
            const labels=typeof rows[row]==="string"?Array.from(rows[row]):rows[row];
            for(let col=0;col<labels.length;col++){
                const value=labels[col],button=document.createElement("button");button.type="button";button.textContent=upper && value.length===1?value.toUpperCase():value;
                Object.assign(button.style,{flex:"1",padding:"8px",fontSize:(19*scale)+"px",color:options.textColor,background:options.surfaceColor,border:"2px solid "+options.focusColor,borderRadius:corners+"px"});
                const index=keys.length;keys.push({button,value,row,col});button.addEventListener("click",()=>{keyIndex=index;typeKey();});line.appendChild(button);
            }
        }
        keyIndex=Math.min(keyIndex,keys.length-1);highlightKey();
    }
    function highlightKey(){for(let i=0;i<keys.length;i++){keys[i].button.style.outline=i===keyIndex?"3px solid "+options.focusColor:"none";keys[i].button.style.fontWeight=i===keyIndex?"bold":"normal";}}
    function typeKey(){
        const value=keys[keyIndex].value;
        if(value==="Cancel"){closeKeyboard(false);return;}
        if(value==="Done"){closeKeyboard(true);return;}
        if(value==="Shift")upper=!upper;else if(value==="Delete")draft=Array.from(draft).slice(0,-1).join("");else if(draft.length<Math.min(2048,current.maxLength>=0?current.maxLength:2048))draft+=value==="Space"?" ":upper?value.toUpperCase():value;
        renderKeyboard();
    }
    function openKeyboard(){
        if(!textField(current))return false;
        original=current.isContentEditable?current.textContent:current.value;draft=String(original || "").slice(0,2048);keyIndex=0;upper=false;
        keyboard=box({position:"fixed",zIndex:"2147483647",bottom:"48px",left:"5%",right:"5%",padding:"12px",background:options.surfaceColor,color:options.textColor,border:"3px solid "+options.focusColor,borderRadius:corners+"px",fontSize:(22*scale)+"px"});
        keyboard.setAttribute("data-isolde-remote-keyboard","");renderKeyboard();current.scrollIntoView({block:"start",behavior:"instant"});paint();return true;
    }
    function activate(){
        if(!visible(current) || !candidates().includes(current))select(candidates()[0]);if(!current)return;
        if(textField(current)){openKeyboard();return;}
        if(current.tagName==="SELECT"){
            current.selectedIndex=(current.selectedIndex+1)%Math.max(1,current.options.length);current.dispatchEvent(new Event("change",{bubbles:true}));
        }else current.click();paint();
    }
    const api={
        handle(command){
            if(pageMenu){if(command==="back" || command==="menu")return closeMenu();if(command==="up")menuIndex=Math.max(0,menuIndex-1);else if(command==="down")menuIndex=Math.min(3,menuIndex+1);else if(command==="confirm"){menuSelect();return true;}menuPaint();return true;}
            if(keyboard){
                if(command==="back")return closeKeyboard(false);
                if(command==="confirm"){typeKey();return true;}
                const key=keys[keyIndex];if(command==="left")keyIndex=Math.max(0,keyIndex-1);else if(command==="right")keyIndex=Math.min(keys.length-1,keyIndex+1);
                else if(command==="up" || command==="down"){const row=Math.max(0,Math.min(rows.length-1,key.row+(command==="up"?-1:1))),line=keys.filter(k=>k.row===row);keyIndex=keys.indexOf(line[Math.min(key.col,line.length-1)]);}
                highlightKey();return true;
            }
            if(command==="menu")return openMenu();
            if(command==="back"){if(history.length>1){history.back();return true;}return false;}
            if(command==="confirm"){activate();return true;}
            if(command==="search")return openKeyboard();
            if(!["up","down","left","right"].includes(command))return false;
            move(command);
            return true;
        },
        state(){return {mode:"focus",keyboardOpen:!!keyboard,pageActionsOpen:!!pageMenu,selected:current?current.tagName:""};}
    };
    Object.defineProperty(globalThis,"__isoldeRemote",{value:api,configurable:false,writable:false});
    // Dynamic pages often replace their focused card or open a dialog without
    // moving browser focus. Restore a real control, never a screen coordinate.
    let queued=false;
    const observer=new MutationObserver(records=>{
        if(records.every(record=>owned.has(record.target) || record.target.parentElement && record.target.closest('[data-isolde-remote-keyboard],[data-isolde-remote-menu]')))return;
        if(queued || keyboard || pageMenu)return;queued=true;
        requestAnimationFrame(()=>{queued=false;const all=candidates();if(!all.includes(current))select(all[0]);else paint();});
    });
    observer.observe(root,{childList:true,subtree:true,attributes:true,attributeFilter:['hidden','disabled','inert','aria-hidden','aria-disabled','aria-modal','open']});
    addEventListener("keydown",event=>{
        if(event.ctrlKey || event.altKey || event.metaKey || event.key==="Home")return;
        const commands={ArrowUp:"up",ArrowDown:"down",ArrowLeft:"left",ArrowRight:"right",Enter:"confirm",Escape:"back",BrowserBack:"back",ContextMenu:"menu",F2:"menu",BrowserSearch:"search"};
        if(commands[event.key] && api.handle(commands[event.key])){event.preventDefault();event.stopImmediatePropagation();}
    },true);
    addEventListener("scroll",paint,true);addEventListener("resize",paint);addEventListener("focusin",event=>{if(!keyboard && !pageMenu && candidates().includes(event.target)){current=event.target;paint();}},true);
    return api;
}
