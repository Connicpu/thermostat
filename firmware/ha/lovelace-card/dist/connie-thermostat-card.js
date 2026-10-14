var L2=Object.defineProperty;var M2=(e,C,H)=>C in e?L2(e,C,{enumerable:!0,configurable:!0,writable:!0,value:H}):e[C]=H;var k=(e,C,H)=>M2(e,typeof C!="symbol"?C+"":C,H);var z=globalThis,K=z.ShadowRoot&&(z.ShadyCSS===void 0||z.ShadyCSS.nativeShadow)&&"adoptedStyleSheets"in Document.prototype&&"replace"in CSSStyleSheet.prototype,i1=Symbol(),T1=new WeakMap,w=class{constructor(C,H,V){if(this._$cssResult$=!0,V!==i1)throw Error("CSSResult is not constructable. Use `unsafeCSS` or `css` instead.");this.cssText=C,this.t=H}get styleSheet(){let C=this.o,H=this.t;if(K&&C===void 0){let V=H!==void 0&&H.length===1;V&&(C=T1.get(H)),C===void 0&&((this.o=C=new CSSStyleSheet).replaceSync(this.cssText),V&&T1.set(H,C))}return C}toString(){return this.cssText}},F1=e=>new w(typeof e=="string"?e:e+"",void 0,i1),y=(e,...C)=>{let H=e.length===1?e[0]:C.reduce((V,L,M)=>V+(r=>{if(r._$cssResult$===!0)return r.cssText;if(typeof r=="number")return r;throw Error("Value passed to 'css' function must be a 'css' function result: "+r+". Use 'unsafeCSS' to pass non-literal values, but take care to ensure page security.")})(L)+e[M+1],e[0]);return new w(H,e,i1)},R1=(e,C)=>{if(K)e.adoptedStyleSheets=C.map(H=>H instanceof CSSStyleSheet?H:H.styleSheet);else for(let H of C){let V=document.createElement("style"),L=z.litNonce;L!==void 0&&V.setAttribute("nonce",L),V.textContent=H.cssText,e.appendChild(V)}},A1=K?e=>e:e=>e instanceof CSSStyleSheet?(C=>{let H="";for(let V of C.cssRules)H+=V.cssText;return F1(H)})(e):e;var{is:r2,defineProperty:e2,getOwnPropertyDescriptor:t2,getOwnPropertyNames:i2,getOwnPropertySymbols:A2,getPrototypeOf:o2}=Object,q=globalThis,_1=q.trustedTypes,a2=_1?_1.emptyScript:"",d2=q.reactiveElementPolyfillSupport,T=(e,C)=>e,o1={toAttribute(e,C){switch(C){case Boolean:e=e?a2:null;break;case Object:case Array:e=e==null?e:JSON.stringify(e)}return e},fromAttribute(e,C){let H=e;switch(C){case Boolean:H=e!==null;break;case Number:H=e===null?null:Number(e);break;case Object:case Array:try{H=JSON.parse(e)}catch{H=null}}return H}},E1=(e,C)=>!r2(e,C),D1={attribute:!0,type:String,converter:o1,reflect:!1,useDefault:!1,hasChanged:E1};Symbol.metadata??=Symbol("metadata"),q.litPropertyMetadata??=new WeakMap;var S=class extends HTMLElement{static addInitializer(C){this._$Ei(),(this.l??=[]).push(C)}static get observedAttributes(){return this.finalize(),this._$Eh&&[...this._$Eh.keys()]}static createProperty(C,H=D1){if(H.state&&(H.attribute=!1),this._$Ei(),this.prototype.hasOwnProperty(C)&&((H=Object.create(H)).wrapped=!0),this.elementProperties.set(C,H),!H.noAccessor){let V=Symbol(),L=this.getPropertyDescriptor(C,V,H);L!==void 0&&e2(this.prototype,C,L)}}static getPropertyDescriptor(C,H,V){let{get:L,set:M}=t2(this.prototype,C)??{get(){return this[H]},set(r){this[H]=r}};return{get:L,set(r){let i=L?.call(this);M?.call(this,r),this.requestUpdate(C,i,V)},configurable:!0,enumerable:!0}}static getPropertyOptions(C){return this.elementProperties.get(C)??D1}static _$Ei(){if(this.hasOwnProperty(T("elementProperties")))return;let C=o2(this);C.finalize(),C.l!==void 0&&(this.l=[...C.l]),this.elementProperties=new Map(C.elementProperties)}static finalize(){if(this.hasOwnProperty(T("finalized")))return;if(this.finalized=!0,this._$Ei(),this.hasOwnProperty(T("properties"))){let H=this.properties,V=[...i2(H),...A2(H)];for(let L of V)this.createProperty(L,H[L])}let C=this[Symbol.metadata];if(C!==null){let H=litPropertyMetadata.get(C);if(H!==void 0)for(let[V,L]of H)this.elementProperties.set(V,L)}this._$Eh=new Map;for(let[H,V]of this.elementProperties){let L=this._$Eu(H,V);L!==void 0&&this._$Eh.set(L,H)}this.elementStyles=this.finalizeStyles(this.styles)}static finalizeStyles(C){let H=[];if(Array.isArray(C)){let V=new Set(C.flat(1/0).reverse());for(let L of V)H.unshift(A1(L))}else C!==void 0&&H.push(A1(C));return H}static _$Eu(C,H){let V=H.attribute;return V===!1?void 0:typeof V=="string"?V:typeof C=="string"?C.toLowerCase():void 0}constructor(){super(),this._$Ep=void 0,this.isUpdatePending=!1,this.hasUpdated=!1,this._$Em=null,this._$Ev()}_$Ev(){this._$ES=new Promise(C=>this.enableUpdating=C),this._$AL=new Map,this._$E_(),this.requestUpdate(),this.constructor.l?.forEach(C=>C(this))}addController(C){(this._$EO??=new Set).add(C),this.renderRoot!==void 0&&this.isConnected&&C.hostConnected?.()}removeController(C){this._$EO?.delete(C)}_$E_(){let C=new Map,H=this.constructor.elementProperties;for(let V of H.keys())this.hasOwnProperty(V)&&(C.set(V,this[V]),delete this[V]);C.size>0&&(this._$Ep=C)}createRenderRoot(){let C=this.shadowRoot??this.attachShadow(this.constructor.shadowRootOptions);return R1(C,this.constructor.elementStyles),C}connectedCallback(){this.renderRoot??=this.createRenderRoot(),this.enableUpdating(!0),this._$EO?.forEach(C=>C.hostConnected?.())}enableUpdating(C){}disconnectedCallback(){this._$EO?.forEach(C=>C.hostDisconnected?.())}attributeChangedCallback(C,H,V){this._$AK(C,V)}_$ET(C,H){let V=this.constructor.elementProperties.get(C),L=this.constructor._$Eu(C,V);if(L!==void 0&&V.reflect===!0){let M=(V.converter?.toAttribute!==void 0?V.converter:o1).toAttribute(H,V.type);this._$Em=C,M==null?this.removeAttribute(L):this.setAttribute(L,M),this._$Em=null}}_$AK(C,H){let V=this.constructor,L=V._$Eh.get(C);if(L!==void 0&&this._$Em!==L){let M=V.getPropertyOptions(L),r=typeof M.converter=="function"?{fromAttribute:M.converter}:M.converter?.fromAttribute!==void 0?M.converter:o1;this._$Em=L;let i=r.fromAttribute(H,M.type);this[L]=i??this._$Ej?.get(L)??i,this._$Em=null}}requestUpdate(C,H,V,L=!1,M){if(C!==void 0){let r=this.constructor;if(L===!1&&(M=this[C]),V??=r.getPropertyOptions(C),!((V.hasChanged??E1)(M,H)||V.useDefault&&V.reflect&&M===this._$Ej?.get(C)&&!this.hasAttribute(r._$Eu(C,V))))return;this.C(C,H,V)}this.isUpdatePending===!1&&(this._$ES=this._$EP())}C(C,H,{useDefault:V,reflect:L,wrapped:M},r){V&&!(this._$Ej??=new Map).has(C)&&(this._$Ej.set(C,r??H??this[C]),M!==!0||r!==void 0)||(this._$AL.has(C)||(this.hasUpdated||V||(H=void 0),this._$AL.set(C,H)),L===!0&&this._$Em!==C&&(this._$Eq??=new Set).add(C))}async _$EP(){this.isUpdatePending=!0;try{await this._$ES}catch(H){Promise.reject(H)}let C=this.scheduleUpdate();return C!=null&&await C,!this.isUpdatePending}scheduleUpdate(){return this.performUpdate()}performUpdate(){if(!this.isUpdatePending)return;if(!this.hasUpdated){if(this.renderRoot??=this.createRenderRoot(),this._$Ep){for(let[L,M]of this._$Ep)this[L]=M;this._$Ep=void 0}let V=this.constructor.elementProperties;if(V.size>0)for(let[L,M]of V){let{wrapped:r}=M,i=this[L];r!==!0||this._$AL.has(L)||i===void 0||this.C(L,void 0,M,i)}}let C=!1,H=this._$AL;try{C=this.shouldUpdate(H),C?(this.willUpdate(H),this._$EO?.forEach(V=>V.hostUpdate?.()),this.update(H)):this._$EM()}catch(V){throw C=!1,this._$EM(),V}C&&this._$AE(H)}willUpdate(C){}_$AE(C){this._$EO?.forEach(H=>H.hostUpdated?.()),this.hasUpdated||(this.hasUpdated=!0,this.firstUpdated(C)),this.updated(C)}_$EM(){this._$AL=new Map,this.isUpdatePending=!1}get updateComplete(){return this.getUpdateComplete()}getUpdateComplete(){return this._$ES}shouldUpdate(C){return!0}update(C){this._$Eq&&=this._$Eq.forEach(H=>this._$ET(H,this[H])),this._$EM()}updated(C){}firstUpdated(C){}};S.elementStyles=[],S.shadowRootOptions={mode:"open"},S[T("elementProperties")]=new Map,S[T("finalized")]=new Map,d2?.({ReactiveElement:S}),(q.reactiveElementVersions??=[]).push("2.1.2");var n1=globalThis,$1=e=>e,j=n1.trustedTypes,N1=j?j.createPolicy("lit-html",{createHTML:e=>e}):void 0,z1="$lit$",u=`lit$${Math.random().toFixed(9).slice(2)}$`,K1="?"+u,p2=`<${K1}>`,O=document,R=()=>O.createComment(""),_=e=>e===null||typeof e!="object"&&typeof e!="function",l1=Array.isArray,m2=e=>l1(e)||typeof e?.[Symbol.iterator]=="function",a1=`[ 	
\f\r]`,F=/<(?:(!--|\/[^a-zA-Z])|(\/?[a-zA-Z][^>\s]*)|(\/?$))/g,W1=/-->/g,I1=/>/g,c=RegExp(`>|${a1}(?:([^\\s"'>=/]+)(${a1}*=${a1}*(?:[^ 	
\f\r"'\`<>=]|("|')|))|$)`,"g"),U1=/'/g,Q1=/"/g,q1=/^(?:script|style|textarea|title)$/i,Z1=e=>(C,...H)=>({_$litType$:e,strings:C,values:H}),A=Z1(1),j1=Z1(2),b2=Z1(3),g=Symbol.for("lit-noChange"),o=Symbol.for("lit-nothing"),G1=new WeakMap,h=O.createTreeWalker(O,129);function X1(e,C){if(!l1(e)||!e.hasOwnProperty("raw"))throw Error("invalid template strings array");return N1!==void 0?N1.createHTML(C):C}var v2=(e,C)=>{let H=e.length-1,V=[],L,M=C===2?"<svg>":C===3?"<math>":"",r=F;for(let i=0;i<H;i++){let t=e[i],a,p,d=-1,v=0;for(;v<t.length&&(r.lastIndex=v,p=r.exec(t),p!==null);)v=r.lastIndex,r===F?p[1]==="!--"?r=W1:p[1]!==void 0?r=I1:p[2]!==void 0?(q1.test(p[2])&&(L=RegExp("</"+p[2],"g")),r=c):p[3]!==void 0&&(r=c):r===c?p[0]===">"?(r=L??F,d=-1):p[1]===void 0?d=-2:(d=r.lastIndex-p[2].length,a=p[1],r=p[3]===void 0?c:p[3]==='"'?Q1:U1):r===Q1||r===U1?r=c:r===W1||r===I1?r=F:(r=c,L=void 0);let n=r===c&&e[i+1].startsWith("/>")?" ":"";M+=r===F?t+p2:d>=0?(V.push(a),t.slice(0,d)+z1+t.slice(d)+u+n):t+u+(d===-2?i:n)}return[X1(e,M+(e[H]||"<?>")+(C===2?"</svg>":C===3?"</math>":"")),V]},D=class e{constructor({strings:C,_$litType$:H},V){let L;this.parts=[];let M=0,r=0,i=C.length-1,t=this.parts,[a,p]=v2(C,H);if(this.el=e.createElement(a,V),h.currentNode=this.el.content,H===2||H===3){let d=this.el.content.firstChild;d.replaceWith(...d.childNodes)}for(;(L=h.nextNode())!==null&&t.length<i;){if(L.nodeType===1){if(L.hasAttributes())for(let d of L.getAttributeNames())if(d.endsWith(z1)){let v=p[r++],n=L.getAttribute(d).split(u),x=/([.?@])?(.*)/.exec(v);t.push({type:1,index:M,name:x[2],strings:n,ctor:x[1]==="."?p1:x[1]==="?"?m1:x[1]==="@"?v1:B}),L.removeAttribute(d)}else d.startsWith(u)&&(t.push({type:6,index:M}),L.removeAttribute(d));if(q1.test(L.tagName)){let d=L.textContent.split(u),v=d.length-1;if(v>0){L.textContent=j?j.emptyScript:"";for(let n=0;n<v;n++)L.append(d[n],R()),h.nextNode(),t.push({type:2,index:++M});L.append(d[v],R())}}}else if(L.nodeType===8)if(L.data===K1)t.push({type:2,index:M});else{let d=-1;for(;(d=L.data.indexOf(u,d+1))!==-1;)t.push({type:7,index:M}),d+=u.length-1}M++}}static createElement(C,H){let V=O.createElement("template");return V.innerHTML=C,V}};function b(e,C,H=e,V){if(C===g)return C;let L=V!==void 0?H._$Co?.[V]:H._$Cl,M=_(C)?void 0:C._$litDirective$;return L?.constructor!==M&&(L?._$AO?.(!1),M===void 0?L=void 0:(L=new M(e),L._$AT(e,H,V)),V!==void 0?(H._$Co??=[])[V]=L:H._$Cl=L),L!==void 0&&(C=b(e,L._$AS(e,C.values),L,V)),C}var d1=class{constructor(C,H){this._$AV=[],this._$AN=void 0,this._$AD=C,this._$AM=H}get parentNode(){return this._$AM.parentNode}get _$AU(){return this._$AM._$AU}u(C){let{el:{content:H},parts:V}=this._$AD,L=(C?.creationScope??O).importNode(H,!0);h.currentNode=L;let M=h.nextNode(),r=0,i=0,t=V[0];for(;t!==void 0;){if(r===t.index){let a;t.type===2?a=new E(M,M.nextSibling,this,C):t.type===1?a=new t.ctor(M,t.name,t.strings,this,C):t.type===6&&(a=new x1(M,this,C)),this._$AV.push(a),t=V[++i]}r!==t?.index&&(M=h.nextNode(),r++)}return h.currentNode=O,L}p(C){let H=0;for(let V of this._$AV)V!==void 0&&(V.strings!==void 0?(V._$AI(C,V,H),H+=V.strings.length-2):V._$AI(C[H])),H++}},E=class e{get _$AU(){return this._$AM?._$AU??this._$Cv}constructor(C,H,V,L){this.type=2,this._$AH=o,this._$AN=void 0,this._$AA=C,this._$AB=H,this._$AM=V,this.options=L,this._$Cv=L?.isConnected??!0}get parentNode(){let C=this._$AA.parentNode,H=this._$AM;return H!==void 0&&C?.nodeType===11&&(C=H.parentNode),C}get startNode(){return this._$AA}get endNode(){return this._$AB}_$AI(C,H=this){C=b(this,C,H),_(C)?C===o||C==null||C===""?(this._$AH!==o&&this._$AR(),this._$AH=o):C!==this._$AH&&C!==g&&this._(C):C._$litType$!==void 0?this.$(C):C.nodeType!==void 0?this.T(C):m2(C)?this.k(C):this._(C)}O(C){return this._$AA.parentNode.insertBefore(C,this._$AB)}T(C){this._$AH!==C&&(this._$AR(),this._$AH=this.O(C))}_(C){this._$AH!==o&&_(this._$AH)?this._$AA.nextSibling.data=C:this.T(O.createTextNode(C)),this._$AH=C}$(C){let{values:H,_$litType$:V}=C,L=typeof V=="number"?this._$AC(C):(V.el===void 0&&(V.el=D.createElement(X1(V.h,V.h[0]),this.options)),V);if(this._$AH?._$AD===L)this._$AH.p(H);else{let M=new d1(L,this),r=M.u(this.options);M.p(H),this.T(r),this._$AH=M}}_$AC(C){let H=G1.get(C.strings);return H===void 0&&G1.set(C.strings,H=new D(C)),H}k(C){l1(this._$AH)||(this._$AH=[],this._$AR());let H=this._$AH,V,L=0;for(let M of C)L===H.length?H.push(V=new e(this.O(R()),this.O(R()),this,this.options)):V=H[L],V._$AI(M),L++;L<H.length&&(this._$AR(V&&V._$AB.nextSibling,L),H.length=L)}_$AR(C=this._$AA.nextSibling,H){for(this._$AP?.(!1,!0,H);C!==this._$AB;){let V=$1(C).nextSibling;$1(C).remove(),C=V}}setConnected(C){this._$AM===void 0&&(this._$Cv=C,this._$AP?.(C))}},B=class{get tagName(){return this.element.tagName}get _$AU(){return this._$AM._$AU}constructor(C,H,V,L,M){this.type=1,this._$AH=o,this._$AN=void 0,this.element=C,this.name=H,this._$AM=L,this.options=M,V.length>2||V[0]!==""||V[1]!==""?(this._$AH=Array(V.length-1).fill(new String),this.strings=V):this._$AH=o}_$AI(C,H=this,V,L){let M=this.strings,r=!1;if(M===void 0)C=b(this,C,H,0),r=!_(C)||C!==this._$AH&&C!==g,r&&(this._$AH=C);else{let i=C,t,a;for(C=M[0],t=0;t<M.length-1;t++)a=b(this,i[V+t],H,t),a===g&&(a=this._$AH[t]),r||=!_(a)||a!==this._$AH[t],a===o?C=o:C!==o&&(C+=(a??"")+M[t+1]),this._$AH[t]=a}r&&!L&&this.j(C)}j(C){C===o?this.element.removeAttribute(this.name):this.element.setAttribute(this.name,C??"")}},p1=class extends B{constructor(){super(...arguments),this.type=3}j(C){this.element[this.name]=C===o?void 0:C}},m1=class extends B{constructor(){super(...arguments),this.type=4}j(C){this.element.toggleAttribute(this.name,!!C&&C!==o)}},v1=class extends B{constructor(C,H,V,L,M){super(C,H,V,L,M),this.type=5}_$AI(C,H=this){if((C=b(this,C,H,0)??o)===g)return;let V=this._$AH,L=C===o&&V!==o||C.capture!==V.capture||C.once!==V.once||C.passive!==V.passive,M=C!==o&&(V===o||L);L&&this.element.removeEventListener(this.name,this,V),M&&this.element.addEventListener(this.name,this,C),this._$AH=C}handleEvent(C){typeof this._$AH=="function"?this._$AH.call(this.options?.host??this.element,C):this._$AH.handleEvent(C)}},x1=class{constructor(C,H,V){this.element=C,this.type=6,this._$AN=void 0,this._$AM=H,this.options=V}get _$AU(){return this._$AM._$AU}_$AI(C){b(this,C)}};var x2=n1.litHtmlPolyfillSupport;x2?.(D,E),(n1.litHtmlVersions??=[]).push("3.3.3");var Y1=(e,C,H)=>{let V=H?.renderBefore??C,L=V._$litPart$;if(L===void 0){let M=H?.renderBefore??null;V._$litPart$=L=new E(C.insertBefore(R(),M),M,void 0,H??{})}return L._$AI(e),L};var S1=globalThis,Z=class extends S{constructor(){super(...arguments),this.renderOptions={host:this},this._$Do=void 0}createRenderRoot(){let C=super.createRenderRoot();return this.renderOptions.renderBefore??=C.firstChild,C}update(C){let H=this.render();this.hasUpdated||(this.renderOptions.isConnected=this.isConnected),super.update(C),this._$Do=Y1(H,this.renderRoot,this.renderOptions)}connectedCallback(){super.connectedCallback(),this._$Do?.setConnected(!0)}disconnectedCallback(){super.disconnectedCallback(),this._$Do?.setConnected(!1)}render(){return g}};Z._$litElement$=!0,Z.finalized=!0,S1.litElementHydrateSupport?.({LitElement:Z});var n2=S1.litElementPolyfillSupport;n2?.({LitElement:Z});(S1.litElementVersions??=[]).push("4.2.2");var $="M11,15H13V17H11V15M11,7H13V13H11V7M12,2C6.47,2 2,6.5 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20Z";var u1="M12 2C17.5 2 22 6.5 22 12S17.5 22 12 22 2 17.5 2 12 6.5 2 12 2M12 4C10.1 4 8.4 4.6 7.1 5.7L18.3 16.9C19.3 15.5 20 13.8 20 12C20 7.6 16.4 4 12 4M16.9 18.3L5.7 7.1C4.6 8.4 4 10.1 4 12C4 16.4 7.6 20 12 20C13.9 20 15.6 19.4 16.9 18.3Z";var s1="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z";var c1="M11 7V13L16.2 16.1L17 14.9L12.5 12.2V7H11M20 12V18H22V12H20M20 20V22H22V20H20M18 20C16.3 21.3 14.3 22 12 22C6.5 22 2 17.5 2 12S6.5 2 12 2C16.8 2 20.9 5.4 21.8 10H19.7C18.8 6.6 15.7 4 12 4C7.6 4 4 7.6 4 12S7.6 20 12 20C14.4 20 16.5 18.9 18 17.3V20Z";var X="M12,11A1,1 0 0,0 11,12A1,1 0 0,0 12,13A1,1 0 0,0 13,12A1,1 0 0,0 12,11M12.5,2C17,2 17.11,5.57 14.75,6.75C13.76,7.24 13.32,8.29 13.13,9.22C13.61,9.42 14.03,9.73 14.35,10.13C18.05,8.13 22.03,8.92 22.03,12.5C22.03,17 18.46,17.1 17.28,14.73C16.78,13.74 15.72,13.3 14.79,13.11C14.59,13.59 14.28,14 13.88,14.34C15.87,18.03 15.08,22 11.5,22C7,22 6.91,18.42 9.27,17.24C10.25,16.75 10.69,15.71 10.89,14.79C10.4,14.59 9.97,14.27 9.65,13.87C5.96,15.85 2,15.07 2,11.5C2,7 5.56,6.89 6.74,9.26C7.24,10.25 8.29,10.68 9.22,10.87C9.41,10.39 9.73,9.97 10.14,9.65C8.15,5.96 8.94,2 12.5,2Z";var N="M17.66 11.2C17.43 10.9 17.15 10.64 16.89 10.38C16.22 9.78 15.46 9.35 14.82 8.72C13.33 7.26 13 4.85 13.95 3C13 3.23 12.17 3.75 11.46 4.32C8.87 6.4 7.85 10.07 9.07 13.22C9.11 13.32 9.15 13.42 9.15 13.55C9.15 13.77 9 13.97 8.8 14.05C8.57 14.15 8.33 14.09 8.14 13.93C8.08 13.88 8.04 13.83 8 13.76C6.87 12.33 6.69 10.28 7.45 8.64C5.78 10 4.87 12.3 5 14.47C5.06 14.97 5.12 15.47 5.29 15.97C5.43 16.57 5.7 17.17 6 17.7C7.08 19.43 8.95 20.67 10.96 20.92C13.1 21.19 15.39 20.8 17.03 19.32C18.86 17.66 19.5 15 18.56 12.72L18.43 12.46C18.22 12 17.66 11.2 17.66 11.2M14.5 17.5C14.22 17.74 13.76 18 13.4 18.1C12.28 18.5 11.16 17.94 10.5 17.28C11.69 17 12.4 16.12 12.61 15.23C12.78 14.43 12.46 13.77 12.33 13C12.21 12.26 12.23 11.63 12.5 10.94C12.69 11.32 12.89 11.7 13.13 12C13.9 13 15.11 13.44 15.37 14.8C15.41 14.94 15.43 15.08 15.43 15.23C15.46 16.05 15.1 16.95 14.5 17.5H14.5Z";var h1="M11,9H13V7H11M12,20C7.59,20 4,16.41 4,12C4,7.59 7.59,4 12,4C16.41,4 20,7.59 20,12C20,16.41 16.41,20 12,20M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M11,17H13V11H11V17Z";var Y="M19,13H5V11H19V13Z";var J="M19,13H13V19H11V13H5V11H11V5H13V11H19V13Z";var O1="M16.56,5.44L15.11,6.89C16.84,7.94 18,9.83 18,12A6,6 0 0,1 12,18A6,6 0 0,1 6,12C6,9.83 7.16,7.94 8.88,6.88L7.44,5.44C5.36,6.88 4,9.28 4,12A8,8 0 0,0 12,20A8,8 0 0,0 20,12C20,9.28 18.64,6.88 16.56,5.44M13,3H11V13H13";var W="M20.79,13.95L18.46,14.57L16.46,13.44V10.56L18.46,9.43L20.79,10.05L21.31,8.12L19.54,7.65L20,5.88L18.07,5.36L17.45,7.69L15.45,8.82L13,7.38V5.12L14.71,3.41L13.29,2L12,3.29L10.71,2L9.29,3.41L11,5.12V7.38L8.5,8.82L6.5,7.69L5.92,5.36L4,5.88L4.47,7.65L2.7,8.12L3.22,10.05L5.55,9.43L7.55,10.56V13.45L5.55,14.58L3.22,13.96L2.7,15.89L4.47,16.36L4,18.12L5.93,18.64L6.55,16.31L8.55,15.18L11,16.62V18.88L9.29,20.59L10.71,22L12,20.71L13.29,22L14.7,20.59L13,18.88V16.62L15.5,15.17L17.5,16.3L18.12,18.63L20,18.12L19.53,16.35L21.3,15.88L20.79,13.95M9.5,10.56L12,9.11L14.5,10.56V13.44L12,14.89L9.5,13.44V10.56Z";var C1="M17.41 11.83L20.58 15L22 13.59L17.41 9L12.82 13.59L14.24 15L17.41 11.83M10 13V5C10 3.34 8.66 2 7 2S4 3.34 4 5V13C1.79 14.66 1.34 17.79 3 20S7.79 22.66 10 21 12.66 16.21 11 14C10.72 13.62 10.38 13.28 10 13M7 4C7.55 4 8 4.45 8 5V8H6V5C6 4.45 6.45 4 7 4Z";var g1="M15 3H9V1H15V3M11 14H13V8H11V14M19 13C19.7 13 20.36 13.13 21 13.35C21 13.23 21 13.12 21 13C21 10.88 20.26 8.93 19.03 7.39L20.45 5.97C20 5.46 19.55 5 19.04 4.56L17.62 6C16.07 4.74 14.12 4 12 4C7.03 4 3 8.03 3 13S7.03 22 12 22C12.59 22 13.16 21.94 13.71 21.83C13.4 21.25 13.18 20.6 13.08 19.91C12.72 19.96 12.37 20 12 20C8.13 20 5 16.87 5 13S8.13 6 12 6 19 9.13 19 13M17 16V22L22 19L17 16Z";var f1="M12,3.25C12,3.25 6,10 6,14C6,17.32 8.69,20 12,20A6,6 0 0,0 18,14C18,10 12,3.25 12,3.25M14.47,9.97L15.53,11.03L9.53,17.03L8.47,15.97M9.75,10A1.25,1.25 0 0,1 11,11.25A1.25,1.25 0 0,1 9.75,12.5A1.25,1.25 0 0,1 8.5,11.25A1.25,1.25 0 0,1 9.75,10M14.25,14.5A1.25,1.25 0 0,1 15.5,15.75A1.25,1.25 0 0,1 14.25,17A1.25,1.25 0 0,1 13,15.75A1.25,1.25 0 0,1 14.25,14.5Z";var H1="M17.75,4.09L15.22,6.03L16.13,9.09L13.5,7.28L10.87,9.09L11.78,6.03L9.25,4.09L12.44,4L13.5,1L14.56,4L17.75,4.09M21.25,11L19.61,12.25L20.2,14.23L18.5,13.06L16.8,14.23L17.39,12.25L15.75,11L17.81,10.95L18.5,9L19.19,10.95L21.25,11M18.97,15.95C19.8,15.87 20.69,17.05 20.16,17.8C19.84,18.25 19.5,18.67 19.08,19.07C15.17,23 8.84,23 4.94,19.07C1.03,15.17 1.03,8.83 4.94,4.93C5.34,4.53 5.76,4.17 6.21,3.85C6.96,3.32 8.14,4.21 8.06,5.04C7.79,7.9 8.75,10.87 10.95,13.06C13.14,15.26 16.1,16.22 18.97,15.95M17.33,17.97C14.5,17.81 11.7,16.64 9.53,14.5C7.36,12.31 6.2,9.5 6.04,6.68C3.23,9.82 3.34,14.64 6.35,17.66C9.37,20.67 14.19,20.78 17.33,17.97Z";var V1="M12,7A5,5 0 0,1 17,12A5,5 0 0,1 12,17A5,5 0 0,1 7,12A5,5 0 0,1 12,7M12,9A3,3 0 0,0 9,12A3,3 0 0,0 12,15A3,3 0 0,0 15,12A3,3 0 0,0 12,9M12,2L14.39,5.42C13.65,5.15 12.84,5 12,5C11.16,5 10.35,5.15 9.61,5.42L12,2M3.34,7L7.5,6.65C6.9,7.16 6.36,7.78 5.94,8.5C5.5,9.24 5.25,10 5.11,10.79L3.34,7M3.36,17L5.12,13.23C5.26,14 5.53,14.78 5.95,15.5C6.37,16.24 6.91,16.86 7.5,17.37L3.36,17M20.65,7L18.88,10.79C18.74,10 18.47,9.23 18.05,8.5C17.63,7.78 17.1,7.15 16.5,6.64L20.65,7M20.64,17L16.5,17.36C17.09,16.85 17.62,16.22 18.04,15.5C18.46,14.77 18.73,14 18.87,13.21L20.64,17M12,22L9.59,18.56C10.33,18.83 11.14,19 12,19C12.82,19 13.63,18.83 14.37,18.56L12,22Z";var k1="M3,12H7A5,5 0 0,1 12,7A5,5 0 0,1 17,12H21A1,1 0 0,1 22,13A1,1 0 0,1 21,14H3A1,1 0 0,1 2,13A1,1 0 0,1 3,12M15,12A3,3 0 0,0 12,9A3,3 0 0,0 9,12H15M12,2L14.39,5.42C13.65,5.15 12.84,5 12,5C11.16,5 10.35,5.15 9.61,5.42L12,2M3.34,7L7.5,6.65C6.9,7.16 6.36,7.78 5.94,8.5C5.5,9.24 5.25,10 5.11,10.79L3.34,7M20.65,7L18.88,10.79C18.74,10 18.47,9.23 18.05,8.5C17.63,7.78 17.1,7.15 16.5,6.64L20.65,7M12.71,16.3L15.82,19.41C16.21,19.8 16.21,20.43 15.82,20.82C15.43,21.21 14.8,21.21 14.41,20.82L12,18.41L9.59,20.82C9.2,21.21 8.57,21.21 8.18,20.82C7.79,20.43 7.79,19.8 8.18,19.41L11.29,16.3C11.5,16.1 11.74,16 12,16C12.26,16 12.5,16.1 12.71,16.3Z";var J1=y`
  :host {
    --ct-heat: var(--state-climate-heat-color, #ff8100);
    --ct-cool: var(--state-climate-cool-color, #2b9af9);
    --ct-fan: var(--state-climate-fan_only-color, #00bcd4);
    --ct-off: var(--state-climate-off-color, var(--disabled-color, #8a8a8a));
    --ct-accent: var(--ct-off);
    --ct-radius: var(--ha-card-border-radius, 12px);
    --ct-tile: var(--secondary-background-color, rgba(127, 127, 127, 0.08));
    --ct-divider: var(--divider-color, rgba(127, 127, 127, 0.2));
    display: block;
  }
  :host([mode='heat']) { --ct-accent: var(--ct-heat); }
  :host([mode='cool']) { --ct-accent: var(--ct-cool); }
  ha-card { overflow: hidden; }
  svg.icon { width: 24px; height: 24px; fill: currentColor; flex: none; }
  svg.sm { width: 18px; height: 18px; }
  .content { padding: 16px; display: flex; flex-direction: column; gap: 16px; }
  .banner {
    display: flex; align-items: center; gap: 8px; padding: 8px 12px; border-radius: 8px;
    background: rgba(var(--rgb-warning-color, 255, 166, 0), 0.15); color: var(--primary-text-color);
    font-size: 14px;
  }
  .banner svg { color: var(--warning-color, #ffa600); }
  .banner.error { background: rgba(var(--rgb-error-color, 219, 68, 55), 0.15); }
  .banner.error svg { color: var(--error-color, #db4437); }

  /* header */
  .head { display: flex; align-items: flex-start; justify-content: space-between; gap: 12px; }
  .title { font-size: 16px; font-weight: 500; color: var(--primary-text-color); }
  .sub { font-size: 13px; color: var(--secondary-text-color); margin-top: 2px; }
  .room { display: flex; align-items: baseline; gap: 12px; margin-top: 4px; }
  .room .t { font-size: 44px; font-weight: 400; line-height: 1.1; color: var(--primary-text-color); }
  .room .t small { font-size: 20px; color: var(--secondary-text-color); }
  .room .h { display: inline-flex; align-items: center; gap: 2px; color: var(--secondary-text-color); font-size: 15px; }
  .chips { display: flex; flex-wrap: wrap; gap: 6px; justify-content: flex-end; }
  .chip {
    display: inline-flex; align-items: center; gap: 4px; height: 28px; padding: 0 10px 0 8px;
    border-radius: 14px; font-size: 13px; font-weight: 500; color: var(--secondary-text-color);
    background: var(--ct-tile);
  }
  .chip.on { color: var(--c); background: color-mix(in srgb, var(--c) 18%, transparent); }
  .chip.heat { --c: var(--ct-heat); }
  .chip.cool { --c: var(--ct-cool); }
  .chip.fan { --c: var(--ct-fan); }
  .chip.on.fan svg { animation: spin 1.6s linear infinite; }
  @keyframes spin { to { transform: rotate(360deg); } }
  @media (prefers-reduced-motion: reduce) { .chip.on.fan svg { animation: none; } }

  /* setpoint */
  .setpoint {
    border-radius: var(--ct-radius); padding: 14px 12px 12px;
    background: color-mix(in srgb, var(--ct-accent) 12%, transparent);
    display: flex; flex-direction: column; align-items: center; gap: 6px;
  }
  .period {
    display: inline-flex; align-items: center; gap: 6px; font-size: 13px; font-weight: 500;
    padding: 4px 10px; border-radius: 14px; color: var(--ct-accent);
    background: var(--card-background-color, var(--ha-card-background, #fff));
  }
  .sp-row { display: flex; align-items: center; justify-content: center; gap: 20px; width: 100%; }
  .sp-val { border-radius: 8px; outline: none; font-size: 52px; line-height: 1; color: var(--primary-text-color); min-width: 3.4ch; text-align: center; font-variant-numeric: tabular-nums; }
  .sp-val small { font-size: 22px; color: var(--secondary-text-color); }
  .sp-val.pending, .stepper .v.pending { opacity: 0.7; font-style: italic; text-decoration: underline dotted; text-underline-offset: 4px; }
  .caption { font-size: 13px; color: var(--secondary-text-color); text-align: center; }
  .round {
    width: 48px; height: 48px; border-radius: 50%; border: none; cursor: pointer; display: grid; place-items: center;
    background: var(--card-background-color, var(--ha-card-background, #fff)); color: var(--ct-accent);
    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.15);
  }
  .round:disabled { opacity: 0.4; cursor: default; }
  .round:focus-visible, .seg button:focus-visible, .mini:focus-visible, .btn:focus-visible, .fanbtn:focus-visible, .sec-h:focus-visible, .sp-val:focus-visible { outline: 2px solid var(--primary-color); outline-offset: 2px; }
  .off-note { font-size: 14px; color: var(--secondary-text-color); padding: 18px 0; }

  /* modes */
  .controls { display: flex; gap: 8px; align-items: stretch; }
  .seg { flex: 1; display: grid; grid-template-columns: repeat(3, 1fr); gap: 4px; padding: 4px; border-radius: var(--ct-radius); background: var(--ct-tile); }
  .seg button, .fanbtn {
    border: none; background: transparent; cursor: pointer; border-radius: 8px; height: 44px; font: inherit;
    font-size: 13px; font-weight: 500; color: var(--secondary-text-color); display: flex; align-items: center; justify-content: center; gap: 6px;
  }
  .seg button[aria-pressed='true'] { color: var(--c, var(--primary-text-color)); background: var(--card-background-color, var(--ha-card-background, #fff)); box-shadow: 0 1px 2px rgba(0,0,0,0.12); }
  .seg .heat { --c: var(--ct-heat); }
  .seg .cool { --c: var(--ct-cool); }
  .seg .off { --c: var(--primary-text-color); }
  .fanbtn { padding: 0 14px; border-radius: var(--ct-radius); background: var(--ct-tile); height: auto; }
  .fanbtn[aria-pressed='true'] { color: var(--ct-fan); background: color-mix(in srgb, var(--ct-fan) 18%, transparent); }

  /* sections */
  section { border-top: 1px solid var(--ct-divider); padding-top: 12px; }
  .sec-h { display: flex; align-items: center; justify-content: space-between; cursor: pointer; user-select: none; background: none; border: none; width: 100%; padding: 0; color: inherit; font: inherit; }
  .sec-h h3 { margin: 0; font-size: 15px; font-weight: 500; color: var(--primary-text-color); }
  .sec-h .meta { font-size: 13px; color: var(--secondary-text-color); display: flex; align-items: center; gap: 4px; }
  .sec-h svg.chev { transition: transform 0.2s; color: var(--secondary-text-color); }
  section.closed .sec-h svg.chev { transform: rotate(-90deg); }
  .sec-b { margin-top: 12px; display: flex; flex-direction: column; gap: 12px; }
  section.closed .sec-b { display: none; }

  /* info tooltips */
  .period-row { display: inline-flex; align-items: center; gap: 4px; }
  .info-w { position: relative; display: inline-flex; vertical-align: middle; }
  .info { border: none; background: none; padding: 4px; margin: -4px 0; border-radius: 50%; color: var(--secondary-text-color); cursor: pointer; display: grid; place-items: center; }
  .info:hover, .info[aria-expanded='true'] { color: var(--primary-color); }
  .info:focus-visible { outline: 2px solid var(--primary-color); outline-offset: 0; }
  svg.xs { width: 16px; height: 16px; }
  .tip {
    position: absolute; top: calc(100% + 6px); z-index: 2; width: max-content; max-width: min(240px, 70vw);
    padding: 8px 10px; border-radius: 8px; font-size: 13px; font-weight: 400; line-height: 1.35; text-align: left;
    color: var(--card-background-color, #fff); background: var(--primary-text-color, #212121);
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.25); visibility: hidden; opacity: 0; transition: opacity 0.12s;
  }
  .tip.open { visibility: visible; opacity: 1; }
  .info-w.center .tip { left: 50%; transform: translateX(-50%); }
  .info-w.start .tip { left: -4px; }
  .info-w.end .tip { right: -4px; }
  .legend { display: flex; align-items: center; gap: 4px; font-size: 13px; color: var(--secondary-text-color); }

  /* timeline */
  .tl { display: grid; grid-template-columns: 40px 1fr; gap: 6px 8px; align-items: center; font-size: 12px; color: var(--secondary-text-color); }
  .bar { position: relative; height: 14px; border-radius: 7px; overflow: hidden; background: color-mix(in srgb, var(--c) 22%, transparent); }
  .bar.heat { --c: var(--ct-heat); }
  .bar.cool { --c: var(--ct-cool); }
  .bar .day { position: absolute; top: 0; bottom: 0; background: color-mix(in srgb, var(--c) 70%, transparent); }
  .bar .mb { position: absolute; top: 0; bottom: 0; background: repeating-linear-gradient(45deg, var(--c), var(--c) 3px, transparent 3px, transparent 6px); }
  .bar .now { position: absolute; top: -2px; bottom: -2px; width: 2px; background: var(--primary-text-color); }
  .ticks { grid-column: 2; display: flex; justify-content: space-between; font-size: 11px; }

  /* schedule grid */
  .sched { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
  .slot { border-radius: var(--ct-radius); padding: 10px; background: var(--ct-tile); display: flex; flex-direction: column; gap: 8px; border: 2px solid transparent; min-width: 0; }
  .slot.heat { --c: var(--ct-heat); }
  .slot.cool { --c: var(--ct-cool); }
  .slot.active { border-color: var(--c); background: color-mix(in srgb, var(--c) 10%, transparent); }
  .slot-h { display: flex; align-items: center; gap: 6px; font-size: 13px; font-weight: 500; color: var(--primary-text-color); }
  .slot-h svg { color: var(--c); }
  .slot-h .now-tag { margin-left: auto; font-size: 11px; font-weight: 600; color: var(--c); text-transform: uppercase; letter-spacing: 0.04em; }
  .field { display: flex; align-items: center; justify-content: space-between; gap: 6px; font-size: 13px; color: var(--secondary-text-color); }
  input[type='time'] {
    font: inherit; font-size: 14px; color: var(--primary-text-color); background: var(--card-background-color, var(--ha-card-background, #fff));
    border: 1px solid var(--ct-divider); border-radius: 6px; padding: 4px 4px; min-width: 0; width: 100%; max-width: 112px; color-scheme: light dark;
  }
  .stepper { display: inline-flex; align-items: center; gap: 2px; }
  .stepper .v { min-width: 5.2ch; white-space: nowrap; text-align: center; font-size: 14px; color: var(--primary-text-color); font-variant-numeric: tabular-nums; }
  .mini { width: 30px; height: 30px; border-radius: 50%; border: none; background: var(--card-background-color, var(--ha-card-background, #fff)); color: var(--primary-text-color); display: grid; place-items: center; cursor: pointer; }
  .mini:disabled { opacity: 0.35; cursor: default; }
  .mini svg { width: 18px; height: 18px; }
  .rows .mini, .rows select { background: var(--ct-tile); border-color: transparent; }
  .rows { display: flex; flex-direction: column; gap: 10px; }
  .row { display: flex; align-items: center; justify-content: space-between; gap: 8px; font-size: 14px; color: var(--primary-text-color); min-height: 36px; }
  .row .l { display: flex; align-items: center; gap: 4px; }
   .row .r { color: var(--secondary-text-color); }
  .row .r.bad { color: var(--error-color, #db4437); font-weight: 500; }
  select { font: inherit; font-size: 14px; color: var(--primary-text-color); background: var(--card-background-color, var(--ha-card-background, #fff)); border: 1px solid var(--ct-divider); border-radius: 6px; padding: 4px 6px; }
  .btns { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
  .btn { border: none; border-radius: var(--ct-radius); background: var(--ct-tile); color: var(--primary-text-color); font: inherit; font-size: 13px; font-weight: 500; padding: 10px 6px; display: flex; flex-direction: column; align-items: center; gap: 4px; cursor: pointer; }
  .btn svg { color: var(--primary-color); }
  .btn:disabled { opacity: 0.4; cursor: default; }
  .boost-live { display: flex; align-items: center; gap: 8px; padding: 8px 12px; border-radius: 8px; font-size: 14px; color: var(--primary-text-color); background: color-mix(in srgb, var(--primary-color) 14%, transparent); }
  .boost-live svg { color: var(--primary-color); }
  .missing { font-size: 13px; color: var(--secondary-text-color); font-style: italic; }

  @container (min-width: 620px) {
    .wide { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; align-items: start; }
  }
  .wrap { container-type: inline-size; }
`;var b1="Connie",B1="thermostat-v3",I={climate:{domain:"climate"},humidity:{domain:"sensor",deviceClass:"humidity",suffix:"humidity"},day_heat_start:{domain:"number",tk:"day_heat_start",suffix:"day_heating_start"},day_heat_temp:{domain:"number",tk:"day_heat_temp",suffix:"day_heating_temperature"},night_heat_start:{domain:"number",tk:"night_heat_start",suffix:"night_heating_start"},night_heat_temp:{domain:"number",tk:"night_heat_temp",suffix:"night_heating_temperature"},day_cool_start:{domain:"number",tk:"day_cool_start",suffix:"day_cooling_start"},day_cool_temp:{domain:"number",tk:"day_cool_temp",suffix:"day_cooling_temperature"},night_cool_start:{domain:"number",tk:"night_cool_start",suffix:"night_cooling_start"},night_cool_temp:{domain:"number",tk:"night_cool_temp",suffix:"night_cooling_temperature"},morning_boost_duration:{domain:"number",tk:"morning_boost_duration",suffix:"morning_heating_boost_duration"},morning_boost_offset:{domain:"number",tk:"morning_boost_offset",suffix:"morning_heating_boost_offset"},schedule_state:{domain:"sensor",tk:"schedule_state",suffix:"schedule_period"},temperature_swing:{domain:"number",tk:"temperature_swing",suffix:"temperature_swing"},boost_setpoint:{domain:"number",tk:"boost_setpoint",suffix:"boost_target_temperature"},boost_duration:{domain:"number",tk:"boost_duration",suffix:"boost_duration"},boost_mode:{domain:"select",tk:"boost_mode",suffix:"boost_mode"},boost_start_target:{domain:"button",tk:"boost_start_target",suffix:"start_boost_to_target"},boost_start_timed:{domain:"button",tk:"boost_start_timed",suffix:"start_timed_boost"},boost_cancel:{domain:"button",tk:"boost_cancel",suffix:"cancel_boost"},boost_state:{domain:"sensor",tk:"boost_state",suffix:"boost"},boost_active:{domain:"binary_sensor",tk:"boost_active",suffix:"boost_active"},boost_remaining:{domain:"sensor",tk:"boost_remaining",suffix:"boost_time_remaining"},boost_last_result:{domain:"sensor",tk:"boost_last_result",suffix:"last_boost_result"}},Z2=e=>e.split(".",1)[0],S2=(e,C)=>e.endsWith(`_${C}`)||e.endsWith(`.${C}`);function u2(e,C){let H=null,V=-1;for(let[L,M]of Object.entries(I))M.domain===C&&M.suffix&&M.suffix.length>V&&S2(e,M.suffix)&&(H=L,V=M.suffix.length);return H}function U(e){return Object.values(e?.devices??{}).find(H=>H.manufacturer===b1&&H.model===B1)?.id}function L1(e,C,H={}){let V={},L=Object.values(e?.entities??{}).filter(M=>C&&M.device_id===C);for(let[M,r]of Object.entries(I)){let i=L.filter(a=>Z2(a.entity_id)===r.domain),t=r.tk&&i.find(a=>a.translation_key===r.tk);!t&&r.deviceClass&&(t=i.find(a=>e.states[a.entity_id]?.attributes?.device_class===r.deviceClass)),!t&&r.suffix&&(t=i.find(a=>u2(a.entity_id,r.domain)===M)),!t&&!r.tk&&!r.suffix&&(t=i[0]),t&&(V[M]=t.entity_id)}for(let[M,r]of Object.entries(H??{}))r&&(V[M]=r);return V}function C2(e){let C=String(e??"").toLowerCase();return C==="day"?"day":C==="night"?"night":C.includes("morning")||C.includes("boost")?"boost":C.includes("clock")?"noclock":null}var M1=e=>e!=="night",P=e=>Math.round(Number(e)*60);function f(e){if(!Number.isFinite(e))return"--:--";let C=(Math.round(e)%1440+1440)%1440;return`${String(Math.floor(C/60)).padStart(2,"0")}:${String(C%60).padStart(2,"0")}`}function H2(e,C=.25){let H=/^(\d{1,2}):(\d{2})/.exec(e??"");if(!H)return null;let V=Number(H[1])+Number(H[2])/60;return Math.min(23.75,Math.round(V/C)*C)}function V2(e,C){return e==="\xB0C"&&C==="\xB0F"?1.8:1}function P1(e){let C=String(e??1);return C.includes(".")?C.split(".")[1].length:0}function w1(e,C,H,V){let L=V>0?V:1,M=Number.isFinite(C)?C:0,r=M+Math.round((e-M)/L)*L;return Number.isFinite(C)&&(r=Math.max(C,r)),Number.isFinite(H)&&(r=Math.min(H,r)),Number(r.toFixed(P1(L)+2))}var s=e=>String(e??"").replace(/_+$/,"").replace(/_/g," ");function y1(e){let C=String(e??"").toLowerCase();return C.startsWith("rejected")||C.includes("failsafe")||C.includes("limit")}var r1={device_id:"Thermostat device",name:"Name (optional)",show_schedule:"Show program schedule",show_morning_boost:"Show morning boost",show_manual_boost:"Show boost controls",show_status:"Show tuning & status"},e1=["show_schedule","show_morning_boost","show_manual_boost","show_status"];function s2(){return[{name:"device_id",selector:{device:{filter:[{manufacturer:b1,model:B1}]}}},{name:"name",selector:{text:{}}},{type:"grid",name:"",schema:e1.map(e=>({name:e,selector:{boolean:{}}}))},{type:"expandable",name:"entities",title:"Entity overrides (auto-discovered from the device)",schema:Object.entries(I).map(([e,C])=>({name:e,selector:{entity:{domain:C.domain}}}))}]}var Q=class extends Z{setConfig(C){this._config={...C}}_emit(C){this._config=C,this.dispatchEvent(new CustomEvent("config-changed",{detail:{config:C},bubbles:!0,composed:!0}))}_changed(C){let H={...this._config,...C};if(C.entities){let V=Object.fromEntries(Object.entries(C.entities).filter(([,L])=>L));Object.keys(V).length?H.entities=V:delete H.entities}for(let V of["name","device_id"])H[V]||delete H[V];for(let V of e1)H[V]===!0&&delete H[V];this._emit(H)}render(){if(!this.hass||!this._config)return o;let C=this._config.device_id||U(this.hass),H=L1(this.hass,C,this._config.entities),V=Object.keys(I).length,L={...Object.fromEntries(e1.map(r=>[r,!0])),...this._config,device_id:this._config.device_id??"",entities:this._config.entities??{}},M=A`<div class="found">${C?`Found ${Object.keys(H).length} of ${V} entities on this device.`:"No Connie thermostat V3 device found."}</div>`;return customElements.get("ha-form")?A`<ha-form .hass=${this.hass} .data=${L} .schema=${s2()}
          .computeLabel=${r=>r1[r.name]??r.name.replace(/_/g," ")}
          @value-changed=${r=>this._changed(r.detail.value)}></ha-form>${M}`:A`
      <label>${r1.device_id}<input type="text" name="device_id" .value=${L.device_id}
        @change=${r=>this._changed({device_id:r.target.value.trim()})} /></label>
      <label>${r1.name}<input type="text" name="name" .value=${L.name??""}
        @change=${r=>this._changed({name:r.target.value})} /></label>
      ${e1.map(r=>A`<label><input type="checkbox" name=${r} .checked=${L[r]!==!1}
        @change=${i=>this._changed({[r]:i.target.checked})} /> ${r1[r]}</label>`)}
      ${M}`}};k(Q,"properties",{hass:{attribute:!1},_config:{state:!0}}),k(Q,"styles",y`
    .found { font-size: 13px; color: var(--secondary-text-color); margin-top: 8px; }
    label { display: block; margin: 8px 0; }
    input[type='text'] { width: 100%; box-sizing: border-box; }
  `);customElements.get("connie-thermostat-card-editor")||customElements.define("connie-thermostat-card-editor",Q);var H5="1.0.0",c2=500,m=(e,C="")=>A`<svg class="icon ${C}" viewBox="0 0 24 24" aria-hidden="true">${j1`<path d=${e}></path>`}</svg>`,l=e=>{let C=Number.parseFloat(e?.state);return Number.isFinite(C)?C:null},G=class extends Z{static getConfigElement(){return document.createElement("connie-thermostat-card-editor")}static getStubConfig(C){let H=U(C);return H?{device_id:H}:{}}constructor(){super(),this._open={boost:!0,morning:!0,schedule:!0,status:!1},this._tip=null,this._onDocClick=C=>{this._tip&&!C.composedPath().some(H=>H.classList?.contains("info-w")&&H.dataset.tip===this._tip.id)&&(this._tip=null)},this._onDocKey=C=>{C.key==="Escape"&&this._tip&&(this._tip=null)},this._pending={},this._timers={},this.nowMinutes=null}setConfig(C){if(!C||typeof C!="object")throw new Error("Invalid configuration");this._config={show_schedule:!0,show_morning_boost:!0,show_manual_boost:!0,show_status:!0,...C},C.collapsed&&(this._open={...this._open,...Object.fromEntries(C.collapsed.map(H=>[H,!1]))})}getCardSize(){return 10}getGridOptions(){return{columns:12,min_columns:6,rows:"auto"}}connectedCallback(){super.connectedCallback(),document.addEventListener("click",this._onDocClick,!0),document.addEventListener("keydown",this._onDocKey)}disconnectedCallback(){super.disconnectedCallback(),document.removeEventListener("click",this._onDocClick,!0),document.removeEventListener("keydown",this._onDocKey);for(let C of Object.values(this._timers))clearTimeout(C);this._timers={}}get _deviceId(){return this._config?.device_id||U(this.hass)}get _ents(){return L1(this.hass,this._deviceId,this._config?.entities)}_st(C){let H=this._e?.[C];return H?this.hass.states[H]:void 0}get _haUnit(){return this.hass?.config?.unit_system?.temperature||"\xB0C"}_nowMin(){if(Number.isFinite(this.nowMinutes))return this.nowMinutes;let C=this.hass?.config?.time_zone;try{let H=new Intl.DateTimeFormat("en-GB",{timeZone:C,hour:"2-digit",minute:"2-digit",hourCycle:"h23"}).formatToParts(new Date);return Number(H.find(V=>V.type==="hour").value)*60+Number(H.find(V=>V.type==="minute").value)}catch{let H=new Date;return H.getHours()*60+H.getMinutes()}}_val(C,H){return C in this._pending?this._pending[C]:H}willUpdate(C){if(C.has("hass")&&this.hass){this._e=this._ents;let H={...this._pending},V=!1;for(let M of Object.keys(H)){if(this._timers[M])continue;let r=M==="climate"?this._st("climate")?.attributes?.temperature:l(this._st(M));r!=null&&Math.abs(r-H[M])<1e-6&&(delete H[M],V=!0)}V&&(this._pending=H);let L=this._st("climate")?.state;this.setAttribute("mode",L==="heat"||L==="cool"?L:"off")}}_queue(C,H,V){this._pending={...this._pending,[C]:H},clearTimeout(this._timers[C]),this._timers[C]=setTimeout(()=>{delete this._timers[C],V(H),setTimeout(()=>{if(this._pending[C]===H&&!this._timers[C]){let{[C]:L,...M}=this._pending;this._pending=M}},15e3)},c2)}_call(C,H,V){return this.hass.callService(C,H,V)}_setNumber(C,H){this._queue(C,H,V=>this._call("number","set_value",{entity_id:this._e[C],value:V}))}_press(C){this._call("button","press",{entity_id:this._e[C]})}_setMode(C){this._call("climate","set_hvac_mode",{entity_id:this._e.climate,hvac_mode:C})}_toggleFan(){let H=this._st("climate")?.attributes?.fan_mode==="on";this._call("climate","set_fan_mode",{entity_id:this._e.climate,fan_mode:H?"auto":"on"})}get _spStep(){return this._st("climate")?.attributes?.target_temp_step||(this._haUnit==="\xB0F"?1:.5)}_stepSetpoint(C,H=1){let V=this._st("climate")?.attributes??{},L=this._spStep,M=this._val("climate",V.temperature);if(M==null)return;let r=w1(M+C*H*L,V.min_temp,V.max_temp,L);r!==M&&this._queue("climate",r,i=>this._call("climate","set_temperature",{entity_id:this._e.climate,temperature:i}))}_spKey(C){let H=this._st("climate")?.attributes??{},V=this._val("climate",H.temperature),L=this._spStep,M=t=>Number.isFinite(t)&&Number.isFinite(V)?Math.ceil(Math.abs(V-t)/L):0,i={ArrowUp:[1,1],ArrowRight:[1,1],ArrowDown:[-1,1],ArrowLeft:[-1,1],PageUp:[1,5],PageDown:[-1,5],Home:[-1,M(H.min_temp)],End:[1,M(H.max_temp)]}[C.key];i&&(C.preventDefault(),i[1]>0&&this._stepSetpoint(i[0],i[1]))}_toggle(C){this._open={...this._open,[C]:!this._open[C]}}_fmtNum(C,H,V){let L=V==="\xB0F"||V==="\xB0C",M=L&&Number.isInteger(Number(C.toFixed(H)))?String(Math.round(C)):C.toFixed(H);return V?L?`${M}${V}`:`${M} ${V}`:M}_numStepper(C,{delta:H=!1,label:V=""}={}){let L=this._st(C);if(!L)return A`<span class="missing">n/a</span>`;let M=L.attributes??{},r=Number(M.step)||1,i=l(L),t=this._val(C,i),a=i===null,p=H?V2(M.unit_of_measurement,this._haUnit):1,d=H&&p!==1?this._haUnit:M.unit_of_measurement??"",v=t===null?"\u2014":this._fmtNum(t*p,p!==1?1:P1(r),d),n=x=>this._setNumber(C,w1((t??0)+x*r,Number(M.min),Number(M.max),r));return A`<span class="stepper">
      <button class="mini" aria-label="Decrease ${V}" ?disabled=${a||t<=Number(M.min)} @click=${()=>n(-1)}>${m(Y)}</button>
      <span class="v ${C in this._pending?"pending":""}" aria-label="${V}: ${v}${C in this._pending?", sending\u2026":""}">${v}</span>
      <button class="mini" aria-label="Increase ${V}" ?disabled=${a||t>=Number(M.max)} @click=${()=>n(1)}>${m(J)}</button>
    </span>`}_timeInput(C,H){let V=this._st(C);if(!V)return A`<span class="missing">n/a</span>`;let L=this._val(C,l(V)),M=Number(V.attributes?.step)||.25;return A`<input type="time" step=${Math.round(M*3600)} aria-label=${H}
      .value=${L===null?"":f(P(L))}
      @change=${r=>{let i=H2(r.target.value,M);i!==null&&this._setNumber(C,i)}} />`}_info(C,H,{align:V="center",label:L="More info"}={}){let M=this._tip?.id===C,r=t=>{(!this._tip||this._tip.id!==C||t)&&(this._tip={id:C,pinned:t||this._tip?.id===C&&this._tip.pinned})},i=()=>{this._tip?.id===C&&!this._tip.pinned&&(this._tip=null)};return A`<span class="info-w ${V}" data-tip=${C} @mouseenter=${()=>r(!1)} @mouseleave=${i}>
      <button class="info" type="button" aria-label=${L} aria-describedby="tip-${C}" aria-expanded=${M?"true":"false"}
        @focus=${()=>r(!1)} @blur=${i}
        @click=${t=>{t.stopPropagation(),this._tip=M&&this._tip.pinned?null:{id:C,pinned:!0}}}>${m(h1,"xs")}</button>
      <span class="tip ${M?"open":""}" role="tooltip" id="tip-${C}">${H}</span>
    </span>`}_section(C,H,V,L){let M=this._open[C];return A`<section class=${M?"":"closed"}>
      <button class="sec-h" aria-expanded=${M?"true":"false"} @click=${()=>this._toggle(C)}>
        <h3>${H}</h3><span class="meta">${V}${m(s1,"chev sm")}</span>
      </button>
      <div class="sec-b">${M?L():o}</div>
    </section>`}_programInfo(C,H){let V=C==="cool"?"cool":"heat",L=M1(H),M=`${L?"day":"night"}_${V}_start`,r=`${L?"night":"day"}_${V}_start`,i=l(this._st(M)),t=l(this._st(r));return{fam:V,day:L,label:`${L?"Day":"Night"} ${V==="heat"?"heating":"cooling"}`,span:i!==null&&t!==null?`${f(P(i))}\u2013${f(P(t))}`:""}}render(){if(!this._config||!this.hass)return o;let C=this._st("climate");if(!C)return A`<ha-card><div class="content"><div class="banner error">${m($)}
        <span>No Connie thermostat V3 found. Pick the device in the card editor or set <code>device_id</code>.</span></div></div></ha-card>`;let H=C.attributes??{},V=C.state==="unavailable"||C.state==="unknown",L=C.state,M=C2(this._st("schedule_state")?.state),r=this.hass.devices?.[this._deviceId],i=this._config.name||r?.name_by_user||r?.name||H.friendly_name||"Thermostat";return A`<ha-card>
      <div class="content wrap">
        ${this._renderBanners(V,M)}
        ${this._renderHeader(i,C)}
        ${this._renderSetpoint(C,L,M,V)}
        ${this._renderModes(C,L,V)}
        ${this._config.show_manual_boost?this._renderBoost():o}
        ${this._config.show_morning_boost?this._renderMorning(M):o}
        ${this._config.show_schedule?this._renderSchedule(L,M):o}
        ${this._config.show_status?this._renderStatus():o}
      </div>
    </ha-card>`}_renderBanners(C,H){let V=[];C&&V.push(A`<div class="banner error">${m($)}<span>Thermostat is unavailable (not reporting to Zigbee).</span></div>`),H==="noclock"&&V.push(A`<div class="banner">${m(c1)}<span>No time from Home Assistant yet: holding the day program.</span></div>`);let L=this._st("boost_last_result")?.state;return y1(L)&&V.push(A`<div class="banner">${m($)}<span>Last boost: ${s(L)}</span></div>`),V}_renderHeader(C,H){let V=H.attributes,L=V.hvac_action,M=l(this._st("humidity")),r=L==="fan"||V.fan_mode==="on",i=V.current_temperature;return A`<div class="head">
      <div>
        <div class="title">${C}</div>
        <div class="room">
          <span class="t" aria-label="Room temperature">${i??"\u2014"}<small>${this._haUnit}</small></span>
          ${M!==null?A`<span class="h">${m(f1,"sm")}${Math.round(M)}%</span>`:o}
        </div>
        <div class="sub">${this._actionText(L,V.fan_mode)}</div>
      </div>
      <div class="chips" aria-label="Running state">
        <span class="chip heat ${L==="heating"?"on":""}">${m(N,"sm")}Heat</span>
        <span class="chip cool ${L==="cooling"?"on":""}">${m(W,"sm")}Cool</span>
        <span class="chip fan ${r?"on":""}">${m(X,"sm")}Fan</span>
      </div>
    </div>`}_actionText(C,H){let V={heating:"Heating",cooling:"Cooling",fan:"Fan only",idle:"Idle",off:"Off"}[C]??(C?s(C):"");return H==="on"&&C!=="fan"?`${V}${V?" \xB7 ":""}fan always on`:V}_renderSetpoint(C,H,V,L){if(H!=="heat"&&H!=="cool")return A`<div class="setpoint"><div class="off-note">${L?"Unavailable":"System is off. Choose Heat or Cool to run the program."}</div></div>`;let M=C.attributes,r=this._val("climate",M.temperature),i=this._programInfo(H,V),t=V==="boost"?k1:i.day?V1:H1,a=V==="boost"?"Morning boost":V==="noclock"?"Day (no clock)":i.day?"Day":"Night";return A`<div class="setpoint">
      <span class="period-row"><span class="period">${m(t,"sm")}${a}${i.span?A` · ${i.span}`:o}</span>
        ${this._info("setpoint",`Changes the ${i.label} program setpoint${V==="boost"?" (the morning boost is added on top)":""}.`,{label:"About the setpoint"})}</span>
      <div class="sp-row">
        <button class="round" aria-label="Lower setpoint" ?disabled=${L||r<=M.min_temp} @click=${()=>this._stepSetpoint(-1)}>${m(Y)}</button>
        <span class="sp-val ${"climate"in this._pending?"pending":""}" role="slider" tabindex=${L?-1:0}
          aria-label="Setpoint" aria-valuenow=${r??o} aria-valuemin=${M.min_temp??o} aria-valuemax=${M.max_temp??o}
          aria-valuetext="${r??"\u2014"} ${this._haUnit}, ${i.label}${"climate"in this._pending?", sending\u2026":""}"
          aria-describedby="tip-setpoint" aria-disabled=${L?"true":"false"} @keydown=${p=>!L&&this._spKey(p)}>${r??"\u2014"}<small>${this._haUnit}</small></span>
        <button class="round" aria-label="Raise setpoint" ?disabled=${L||r>=M.max_temp} @click=${()=>this._stepSetpoint(1)}>${m(J)}</button>
      </div>
    </div>`}_renderModes(C,H,V){let L=(C.attributes.hvac_modes??["off","heat","cool"]).filter(t=>["off","heat","cool"].includes(t)),M=["off","heat","cool"].filter(t=>L.includes(t)),r={off:["Off",O1],heat:["Heat",N],cool:["Cool",W]},i=C.attributes.fan_mode==="on";return A`<div class="controls">
      <div class="seg" role="group" aria-label="System mode">
        ${M.map(t=>A`<button class=${t} aria-pressed=${H===t?"true":"false"} ?disabled=${V} @click=${()=>this._setMode(t)}>${m(r[t][1],"sm")}${r[t][0]}</button>`)}
      </div>
      ${(C.attributes.fan_modes??["auto","on"]).includes("on")?A`<button class="fanbtn" aria-pressed=${i?"true":"false"} aria-label="Fan always on" ?disabled=${V} @click=${()=>this._toggleFan()}>${m(X,"sm")}Fan ${i?"On":"Auto"}</button>`:o}
    </div>`}_bar(C,H,V){let L=l(this._st(`day_${C}_start`)),M=l(this._st(`night_${C}_start`));if(L===null||M===null)return A`<span></span>`;let r=P(L),i=P(M),t=x=>`${x/1440*100}%`,a=r<=i?[[r,i]]:[[r,1440],[0,i]],p=C==="heat"?l(this._st("morning_boost_duration"))??0:0,d=r+p,v=p>0?d<=1440?[[r,d]]:[[r,1440],[0,d-1440]]:[],n=this._nowMin();return A`<div class="bar ${C}" role="img" aria-label="${C==="heat"?"Heating":"Cooling"}: day ${f(r)} to ${f(i)}">
      ${a.map(([x,t1])=>A`<span class="day" style="left:${t(x)};width:${t(t1-x)}"></span>`)}
      ${v.map(([x,t1])=>A`<span class="mb" style="left:${t(x)};width:${t(t1-x)}"></span>`)}
      ${V===C||V!=="cool"&&C==="heat"?A`<span class="now" style="left:calc(${t(n)} - 1px)"></span>`:o}
    </div>`}_slot(C,H,V,L){let M=V===C&&H==="day"===M1(L),r=C==="heat"?"heating":"cooling";return A`<div class="slot ${C} ${M?"active":""}">
      <div class="slot-h">${m(H==="day"?V1:H1,"sm")}${H==="day"?"Day":"Night"}
        ${M?A`<span class="now-tag">Now</span>`:o}</div>
      <div class="field"><span>Starts</span>${this._timeInput(`${H}_${C}_start`,`${H} ${r} start`)}</div>
      <div class="field"><span>Target</span>${this._numStepper(`${H}_${C}_temp`,{label:`${H} ${r} temperature`})}</div>
    </div>`}_renderSchedule(C,H){let V=this._nowMin(),L=C==="heat"||C==="cool"?`${M1(H)?"Day":"Night"} ${C==="heat"?"heating":"cooling"} active`:"Off";return this._section("schedule","Program schedule",L,()=>A`
      <div class="tl">
        <span>Heat</span>${this._bar("heat",H,C)}
        <span>Cool</span>${this._bar("cool",H,C)}
        <span class="ticks"><span>00</span><span>06</span><span>12</span><span>18</span><span>24</span></span>
      </div>
      <div class="legend">Now ${f(V)}${this._info("timeline",`Solid = day, light = night${l(this._st("morning_boost_duration"))?", hatched = morning boost":""}. The line marks now.`,{align:"start",label:"About the timeline"})}</div>
      <div class="wide">
        <div><div class="slot-h" style="margin:0 0 6px">${m(N,"sm")}Heating</div>
          <div class="sched">${this._slot("heat","day",C,H)}${this._slot("heat","night",C,H)}</div></div>
        <div><div class="slot-h" style="margin:8px 0 6px">${m(W,"sm")}Cooling</div>
          <div class="sched">${this._slot("cool","day",C,H)}${this._slot("cool","night",C,H)}</div></div>
      </div>`)}_renderMorning(C){let H=l(this._st("morning_boost_duration")),V=C==="boost"?"Running now":H?`${H} min`:"Off";return this._section("morning","Morning heating boost",V,()=>A`<div class="rows">
      <div class="row"><span class="l">Duration${this._info("mb-duration","Runs from the day heating start for this long; 0 = off.",{align:"start",label:"About morning boost duration"})}</span>${this._numStepper("morning_boost_duration",{label:"morning boost duration"})}</div>
      <div class="row"><span class="l">Offset${this._info("mb-offset","Added to the day heating target while the morning boost runs.",{align:"start",label:"About morning boost offset"})}</span>${this._numStepper("morning_boost_offset",{delta:!0,label:"morning boost offset"})}</div>
    </div>`)}_renderBoost(){let C=this._st("boost_active")?.state==="on",H=this._st("boost_state")?.state,V=l(this._st("boost_remaining")),L=C?`${s(H)}${V?` \xB7 ${Math.round(V)} min left`:""}`:"Idle",M=this._st("boost_mode");return this._section("boost","Boost",L,()=>A`
      ${C?A`<div class="boost-live">${m(C1)}<span>${s(H)}${V?` \u2014 ${Math.round(V)} min left`:""}</span></div>`:o}
      <div class="rows">
        <div class="row"><span>Target</span>${this._numStepper("boost_setpoint",{label:"boost target"})}</div>
        <div class="row"><span>Timed length</span>${this._numStepper("boost_duration",{label:"boost duration"})}</div>
        <div class="row"><span>Direction</span>${M?A`<select aria-label="Boost direction" @change=${r=>this._call("select","select_option",{entity_id:this._e.boost_mode,option:r.target.value})}>
              ${(M.attributes?.options??[]).map(r=>A`<option .value=${r} ?selected=${r===M.state}>${s(r)}</option>`)}</select>`:A`<span class="missing">n/a</span>`}</div>
      </div>
      <div class="btns">
        <button class="btn" ?disabled=${!this._e.boost_start_target} @click=${()=>this._press("boost_start_target")}>${m(C1)}To target</button>
        <button class="btn" ?disabled=${!this._e.boost_start_timed} @click=${()=>this._press("boost_start_timed")}>${m(g1)}Timed</button>
        <button class="btn" ?disabled=${!this._e.boost_cancel||!C} @click=${()=>this._press("boost_cancel")}>${m(u1)}Cancel</button>
      </div>`)}_renderStatus(){let C=this._st("boost_last_result")?.state,H=this._st("schedule_state")?.state,V=this._st("temperature_swing");return this._section("status","Tuning & status","",()=>A`<div class="rows">
      <div class="row"><span class="l">Temperature swing${this._info("swing","How far (\xB1) the room may drift from the setpoint before heating or cooling switches.",{align:"start",label:"About temperature swing"})}</span>${V?this._numStepper("temperature_swing",{delta:!0,label:"temperature swing"}):A`<span class="missing">n/a</span>`}</div>
      <div class="row"><span>Program period</span><span class="r">${H?s(H):"\u2014"}</span></div>
      <div class="row"><span>Last boost result</span><span class="r ${y1(C)?"bad":""}">${C?s(C):"\u2014"}</span></div>
    </div>`)}};k(G,"properties",{hass:{attribute:!1},_config:{state:!0},_open:{state:!0},_pending:{state:!0},_tip:{state:!0}}),k(G,"styles",J1);customElements.get("connie-thermostat-card")||customElements.define("connie-thermostat-card",G);window.customCards=window.customCards||[];window.customCards.some(e=>e.type==="connie-thermostat-card")||window.customCards.push({type:"connie-thermostat-card",name:"Connie Thermostat V3",description:"Room temperature, modes, fan, program schedule, morning boost and boost for the Connie thermostat V3 (ZHA).",preview:!0,documentationURL:"https://github.com/Connicpu/thermostat/tree/main/firmware/ha/lovelace-card"});export{H5 as CARD_VERSION,G as ConnieThermostatCard};
/*! Bundled license information:

@lit/reactive-element/css-tag.js:
  (**
   * @license
   * Copyright 2019 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

@lit/reactive-element/reactive-element.js:
lit-html/lit-html.js:
lit-element/lit-element.js:
  (**
   * @license
   * Copyright 2017 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/is-server.js:
  (**
   * @license
   * Copyright 2022 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)
*/
