"""Test a CYD AP enabled manually in Settings. Never opens serial or CAN."""
import argparse
import copy
import json
import socket
import time
import urllib.error
import urllib.request

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base',default='http://192.168.4.1')
    parser.add_argument('--exercise-save',action='store_true',help='Preview/save a theme and restore original settings')
    args=parser.parse_args()
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
    checks=[]
    def check(name,passed):
        checks.append({'test':name,'passed':bool(passed)})
        print(f'{name}: {"PASS" if passed else "FAIL"}',flush=True)
        if not passed: raise AssertionError(name)
    def request(path,method='GET',body=None,token=None):
        headers={}
        if token:headers['X-MiniDash-Session']=token
        if body is not None:headers['Content-Type']='application/json'
        req=urllib.request.Request(args.base+path,method=method,headers=headers,
                                   data=json.dumps(body).encode() if body is not None else None)
        try:
            with opener.open(req,timeout=15) as response:return response.status,response.read(),response.url
        except urllib.error.HTTPError as error:return error.code,error.read(),error.url
    def get(path):
        code,body,_=request(path);check('GET '+path,code==200);return json.loads(body)
    check('page served',request('/')[0]==200)
    device=get('/api/device');check('CYD target',device['target']=='minidash_cyd')
    for route in ['config','signals','layouts','themes','defaults','presets','analytics','validation']:
        get('/api/'+route)
    check('captive redirect',request('/generate_204')[2]==args.base+'/')
    check('unknown API remains 404',request('/api/no-such-route')[0]==404)
    packet=bytes.fromhex('123401000001000000000000')+b'\x04test\x07invalid\x00\x00\x01\x00\x01'
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as dns:
        dns.settimeout(3);dns.sendto(packet,('192.168.4.1',53));reply=dns.recv(512)
    check('DNS resolves captive host',reply[:2]==packet[:2] and reply[-4:]==bytes([192,168,4,1]))
    if args.exercise_save:
        original=get('/api/config');token=get('/api/session')['token']
        candidate=copy.deepcopy(original);candidate['common']['theme']=(original['common']['theme']+1)%11
        try:
            code,_,_=request('/api/preview','POST',candidate,'wrong')
            check('session gate',code==403)
            code,_,_=request('/api/preview','POST',candidate,token);check('preview',code==200)
            check('stale revision',request('/api/preview','POST',candidate,token)[0]==409)
            current=get('/api/config')
            check('save',request('/api/config','PUT',current,token)[0]==200)
            check('saved theme visible',get('/api/config')['common']['theme']==candidate['common']['theme'])
        finally:
            current=get('/api/config');original['revision']=current['revision']
            check('restore original settings',request('/api/config','PUT',original,token)[0]==200)
        time.sleep(.3)
    print(json.dumps({'status':'PASS','checks':checks},indent=2))

if __name__=='__main__':main()
