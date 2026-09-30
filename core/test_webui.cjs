/* Frontend logic test, deliberately not a browser/rendering acceptance test. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const path=require('node:path');
const fixture=JSON.parse(fs.readFileSync(process.argv[2],'utf8').replace(/^\uFEFF/,''));
class Element {
    constructor(tag,text=''){this.tagName=tag.toUpperCase();this.textContent=text;this.children=[];this.style={};this.value='';this.disabled=false;this.checked=false}
    append(...nodes){this.children.push(...nodes)}
    replaceChildren(...nodes){this.children=[...nodes]}
    click(){return this.onclick?.()}
}
const ids=['message','general','hardware','pages','add','delete','resetpage','pageform','preview','widgets','themes','customtheme','colors','resettheme','warnings','export','import','resetui','try','save','cancel','reload'];
const elements=Object.fromEntries(ids.map(id=>[id,new Element(['add','delete','resetpage','resettheme','export','resetui','try','save','cancel','reload'].includes(id)?'button':'div')]));
const document={getElementById:id=>elements[id],createElement:tag=>new Element(tag),querySelectorAll:tag=>Object.values(elements).filter(e=>e.tagName===tag.toUpperCase())};
let current=structuredClone(fixture.config),saved=structuredClone(current);
async function fetch(url,options){
    if(!options || !options.method)return {ok:true,json:async()=>structuredClone(url==='/api/session'?{token:'test-token'}:url==='/api/config'?current:fixture[url.slice(5)])};
    const candidate=JSON.parse(options.body);
    if(candidate.revision!==current.revision)return {ok:false,status:409,text:async()=> 'stale'};
    current=url==='/api/cancel'?structuredClone(saved):candidate;current.revision++;
    if(url==='/api/config')saved=structuredClone(current);
    return {ok:true,json:async()=>structuredClone(current)};
}
const html=fs.readFileSync(path.join(__dirname,'../firmware/main/webui.html'),'utf8');
const script=html.match(/<script>([\s\S]*)<\/script>/)[1];
const context={document,fetch,structuredClone,Blob,URL,setTimeout,console};vm.createContext(context);new vm.Script(script).runInContext(context);
const tick=()=>new Promise(resolve=>setImmediate(resolve));
const selectOf=id=>elements[id].children.flatMap(x=>x.children).find(x=>x.tagName==='SELECT');
const change=(select,value)=>{select.value=String(value);select.onchange()};
(async()=>{
    await tick();assert.equal(elements.message.textContent,'Соединение с устройством установлено');
    const rotation=()=>elements.general.children.find(x=>x.textContent==='Поворот экрана').children[0];
    assert.equal(rotation().children.length,4);
    for(const r of [1,2,3,0]) {
        change(rotation(),r);
        assert.equal(elements.preview.style.gridTemplateColumns,'repeat('+(r%2?8:12)+',1fr)');
        assert.equal(elements.preview.style.gridTemplateRows,'repeat('+(r%2?12:8)+',20px)');
    }
    change(rotation(),3); await elements.try.click(); assert.equal(current.platform.rotation,3);
    await elements.cancel.click(); assert.equal(current.platform.rotation,0);
    change(rotation(),1); await elements.save.click(); assert.equal(saved.platform.rotation,1);
    change(rotation(),0); await elements.save.click();
    assert.equal(selectOf('themes').children.length,11);
    assert.equal(selectOf('themes').children[4].textContent,'Matrix');
    change(selectOf('themes'),4);
    assert.equal(elements.preview.children[0].style.borderRadius,'0px');
    assert.match(elements.preview.children[0].style.backgroundImage,/repeating-linear-gradient/);
    change(selectOf('themes'),8);
    assert.equal(elements.preview.children[0].style.borderRadius,'18px');
    assert.equal(selectOf('themes').children[9].textContent,'Volkswagen');
    assert.equal(selectOf('themes').children[10].textContent,'Porsche');
    change(selectOf('themes'),0);
    assert.equal(elements.widgets.children.length,4);assert.equal(elements.delete.disabled,true);
    elements.add.click();elements.add.click();elements.add.click();assert.equal(elements.add.disabled,true);
    assert.equal(selectOf('pages').children.length,4);
    change(selectOf('pageform'),'5');assert.equal(elements.widgets.children.length,8);
    const widget=elements.widgets.children[0], type=widget.children[2].children[0];change(type,1);
    assert.equal(elements.preview.children[0].children[2].textContent,'Bar · NO DATA');
    change(selectOf('pageform'),'0');assert.equal(elements.widgets.children[0].children.length,11);
    await elements.try.click();assert.match(elements.message.textContent,/Preview/);
    await elements.cancel.click();assert.equal(elements.widgets.children.length,4);assert.equal(selectOf('pages').children.length,1);
    elements.add.click();await elements.save.click();assert.match(elements.message.textContent,/Сохранено/);
    elements.delete.click();await elements.cancel.click();assert.equal(selectOf('pages').children.length,2);
    elements.resetui.click();assert.equal(selectOf('pages').children.length,1);
    assert.match(elements.hardware.textContent,/фиксированная/);
    console.log('MINIDASH_WEBUI_LOGIC_TEST status=PASS');
})().catch(error=>{console.error(error);process.exitCode=1});
