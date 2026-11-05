#!/usr/bin/env python3
"""Local COM2 bridge. No third-party dependencies; bounded TC/1 frames."""
import argparse,asyncio,json,os,struct,threading,urllib.request,urllib.parse
import html.parser,ipaddress,socket
MAX=1024
def frame(kind,request,body=b''):
    if isinstance(body,str):body=body.encode('utf-8')
    if len(body)>MAX:
        body=body[:MAX-20].decode('utf-8','ignore').encode()+b' [TRUNCATED]'
    header=bytearray(struct.pack('<2sBBIHH',b'TC',1,kind,request,len(body),0))
    struct.pack_into('<H',header,10,(sum(header[:10])+sum(body))&65535)
    return bytes(header)+body
class Decoder:
    def __init__(self):self.buf=bytearray()
    def feed(self,data):
        self.buf.extend(data);frames=[]
        while len(self.buf)>=2:
            if self.buf[:2]!=b'TC':del self.buf[0];continue
            if len(self.buf)<12:break
            _,version,kind,req,n,checksum=struct.unpack_from('<2sBBIHH',self.buf)
            if version!=1 or n>MAX:del self.buf[0];continue
            if len(self.buf)<12+n:break
            body=bytes(self.buf[12:12+n])
            if (sum(self.buf[:10])+sum(body))&65535!=checksum:del self.buf[0];continue
            del self.buf[:12+n];frames.append((kind,req,body))
        return frames

class PageText(html.parser.HTMLParser):
    """Convierte HTML no confiable a texto; nunca ejecuta JavaScript ni CSS."""
    BLOCK={'address','article','blockquote','br','dd','div','dl','dt','h1','h2','h3',
           'h4','h5','h6','hr','li','ol','p','pre','section','table','tr','ul'}
    SKIP={'script','style','noscript','svg','iframe','form'}
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts=[];self.title=[];self.in_title=False;self.skip=0
    def handle_starttag(self,tag,attrs):
        if tag in self.SKIP:self.skip+=1
        if tag=='title':self.in_title=True
        if not self.skip and tag in self.BLOCK:self.parts.append('\n')
    def handle_endtag(self,tag):
        if tag=='title':self.in_title=False
        if tag in self.SKIP and self.skip:self.skip-=1
        if not self.skip and tag in self.BLOCK:self.parts.append('\n')
    def handle_data(self,data):
        if self.skip:return
        if self.in_title:self.title.append(data)
        else:self.parts.append(data)
    def result(self):
        title=' '.join(' '.join(self.title).split())[:120] or 'Pagina web'
        raw=''.join(self.parts).replace('\r','\n')
        lines=[]
        for line in raw.splitlines():
            compact=' '.join(line.split())
            if compact:lines.append(compact)
        return title,'\n'.join(lines)

def validate_https_url(value):
    parsed=urllib.parse.urlsplit(value)
    if parsed.scheme.lower()!='https' or not parsed.hostname:
        raise ValueError('SOLO SE PERMITEN URL HTTPS PUBLICAS')
    if parsed.username or parsed.password or parsed.fragment:
        raise ValueError('URL CON CREDENCIALES O FRAGMENTO NO PERMITIDA')
    if parsed.port not in (None,443):
        raise ValueError('SOLO SE PERMITE EL PUERTO HTTPS 443')
    host=parsed.hostname.encode('idna').decode('ascii')
    try:
        addresses=[ipaddress.ip_address(host)]
    except ValueError:
        try:
            addresses=[ipaddress.ip_address(item[4][0]) for item in
                       socket.getaddrinfo(host,443,type=socket.SOCK_STREAM)]
        except OSError as exc:
            raise ValueError('NO SE PUDO RESOLVER EL DOMINIO') from exc
    if not addresses or any(not address.is_global for address in addresses):
        raise ValueError('DESTINO LOCAL/PRIVADO BLOQUEADO POR SEGURIDAD')
    netloc=('['+host+']') if ':' in host else host
    if parsed.port==443:netloc+=':443'
    return urllib.parse.urlunsplit(('https',netloc,parsed.path or '/',parsed.query,''))

class SafeHTTPSRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,newurl):
        validate_https_url(newurl)
        return super().redirect_request(req,fp,code,msg,headers,newurl)

def fetch_page(url):
    """Descarga HTTPS con validacion TLS del anfitrion y devuelve texto acotado."""
    safe_url=validate_https_url(url)
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),SafeHTTPSRedirect())
    request=urllib.request.Request(safe_url,headers={
        'User-Agent':'ToolChainOS-TextBrowser/0.1',
        'Accept':'text/html,text/plain,application/xhtml+xml;q=0.9,*/*;q=0.1'})
    with opener.open(request,timeout=12) as response:
        final_url=validate_https_url(response.geturl())
        media=response.headers.get_content_type().lower()
        if media not in ('text/html','text/plain','application/xhtml+xml'):
            raise ValueError('TIPO DE CONTENIDO NO COMPATIBLE: '+media)
        raw=response.read(512*1024+1)
        if len(raw)>512*1024:raise ValueError('PAGINA MAYOR A 512 KiB')
        charset=response.headers.get_content_charset() or 'utf-8'
    try:source=raw.decode(charset,errors='replace')
    except LookupError:source=raw.decode('utf-8',errors='replace')
    if media=='text/plain':
        title=urllib.parse.urlsplit(final_url).hostname or 'Texto'
        body='\n'.join(' '.join(line.split()) for line in source.splitlines() if line.strip())
    else:
        parser=PageText()
        parser.feed(source);parser.close()
        title,body=parser.result()
    result='TITULO: '+title+'\nURL: '+final_url+'\n\n'+body
    encoded=result.encode('utf-8')
    if len(encoded)>MAX-24:encoded=encoded[:MAX-24].decode('utf-8','ignore').encode()+b'\n[CONTENIDO RECORTADO]'
    return encoded.decode('utf-8','strict')

class Ollama:
    def __init__(self,url,model,timeout=100):
        p=urllib.parse.urlsplit(url)
        if p.scheme!='http' or p.hostname not in ('127.0.0.1','localhost','::1') or p.username or p.password or p.query or p.fragment:
            raise ValueError('OLLAMA_URL debe usar HTTP en loopback')
        self.slots=threading.BoundedSemaphore(2)
        self.url=url.rstrip('/');self.model=model;self.timeout=timeout
        self.opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect())
    def http(self,path,body=None):
        data=None if body is None else json.dumps(body).encode()
        req=urllib.request.Request(self.url+path,data=data,headers={'Content-Type':'application/json'})
        if not self.slots.acquire(blocking=False):raise ValueError('HTTP OCUPADO: INFERENCIA ANTERIOR AUN ACTIVA')
        try:
            with self.opener.open(req,timeout=1.5 if path=='/api/tags' else self.timeout) as response:
                raw=response.read(1024*1024+1)
        finally:self.slots.release()
        if len(raw)>1024*1024:raise ValueError('RESPUESTA HTTP EXCEDE 1MiB')
        return json.loads(raw)
    def ready(self):
        if not self.model:raise ValueError('OLLAMA_MODEL NO CONFIGURADO')
        models=self.http('/api/tags')
        names=[m.get('name') for m in models.get('models',[]) if isinstance(m,dict)]
        if self.model not in names:raise ValueError('MODELO NO INSTALADO: '+self.model)
        return 'OLLAMA READY MODEL='+self.model
    def generate(self,prompt):
        self.ready()
        result=self.http('/api/generate',{'model':self.model,'prompt':prompt,'stream':False,'options':{'num_predict':192}})
        if not isinstance(result.get('response'),str) or result.get('done') is not True:
            raise ValueError('JSON DE GENERACION INVALIDO/INCOMPLETO')
        return result['response']
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs):raise ValueError('REDIRECCION HTTP NO PERMITIDA')
async def bridge(socket_path,backend):
    reader,writer=await asyncio.open_unix_connection(socket_path)
    print('COM2 CONNECTED; API LOCAL; MODEL='+backend.model,flush=True)
    decoder=Decoder();jobs={};lock=asyncio.Lock()
    async def reply(kind,req,body):
        async with lock:
            packet=frame(kind,req,body)
            # Pace below UART FIFO throughput; kernel polls COM2 without IRQ.
            for pos in range(0,len(packet),8):
                writer.write(packet[pos:pos+8]);await writer.drain();await asyncio.sleep(.003)
    async def job(kind,req,body):
        try:
            print(f'REQUEST id={req} type={kind} bytes={len(body)}',flush=True)
            if kind==2:
                result=await asyncio.to_thread(backend.ready);response_kind=0x82
            elif kind==6:
                result=await asyncio.to_thread(fetch_page,body.decode('utf-8',errors='strict'));response_kind=0x86
            else:
                result=await asyncio.to_thread(backend.generate,body.decode('utf-8',errors='strict'));response_kind=0x81
            await reply(response_kind,req,result)
            print(f'RESPONSE id={req} bytes={len(result.encode())}',flush=True)
        except asyncio.CancelledError:
            print(f'CANCEL id={req}',flush=True);raise
        except Exception as exc:
            # No prompt text or secrets in logs.
            await reply(0xff,req,type(exc).__name__+': '+str(exc)[:240])
            print(f'ERROR id={req} type={type(exc).__name__}',flush=True)
        finally:jobs.pop(req,None)
    try:
        while data:=await reader.read(256):
            for kind,req,body in decoder.feed(data):
                if kind==3:
                    if req in jobs:jobs[req].cancel()
                elif kind in (1,2,6):
                    if jobs:
                        await reply(0xff,req,'PUENTE OCUPADO')
                    else:jobs[req]=asyncio.create_task(job(kind,req,body))
                else:await reply(0xff,req,'TIPO NO SOPORTADO')
    finally:
        for j in list(jobs.values()):j.cancel()
        if jobs:await asyncio.gather(*list(jobs.values()),return_exceptions=True)
        writer.close();await writer.wait_closed()
def main():
    p=argparse.ArgumentParser();p.add_argument('--socket',default='build/ai.sock')
    p.add_argument('--url',default=os.getenv('OLLAMA_URL','http://127.0.0.1:11434'))
    p.add_argument('--model',default=os.getenv('OLLAMA_MODEL',''));a=p.parse_args()
    try:asyncio.run(bridge(a.socket,Ollama(a.url,a.model)))
    except (OSError,ValueError) as exc:raise SystemExit(str(exc))
if __name__=='__main__':main()
