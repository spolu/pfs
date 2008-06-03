#!/usr/bin/python

import select
import sys
import pybonjour
import re
import time
from threading import Thread
import socket

sd_id = sys.argv[1]
sd_owner = sys.argv[2]
sd_name = sys.argv[3]
pfsd_port = int (sys.argv[4])
tun_port = pfsd_port + 1
new_port = tun_port + 1
threads = []

#######################

class DNSReg (Thread):
    def __init__ (self, name, regtype, port):
        Thread.__init__(self)
        self.name = name
        self.regtype = regtype
        self.port = port
        self.kill = False
        self.sdRef = pybonjour.DNSServiceRegister (name = self.name,
                                                   regtype = self.regtype,
                                                   port = self.port,
                                                   callBack = self.reg_cb)

    def run (self):
        while self.kill == False:
            ready = select.select ([self.sdRef], [], [], 1)
            if self.sdRef in ready[0]:
                pybonjour.DNSServiceProcessResult (self.sdRef)
        self.sdRef.close ()

    def reg_cb (self, sdRef, flags, errorCode, name, regtype, domain):
        if not (errorCode == pybonjour.kDNSServiceErr_NoError):
            print 'Error registering service'
            exit ()

    def stop (self):
        self.kill = True



#######################

class DNSBrowse (Thread):
    def __init__ (self, regtype):
        Thread.__init__ (self)
        self.kill = False
        self.regtype = regtype
        self.browse_sdRef = pybonjour.DNSServiceBrowse(regtype = self.regtype,
                                                callBack = self.browse_cb)
        self.services = {}

    def run (self):
        while self.kill == False:
            ready = select.select ([self.browse_sdRef], [], [], 1)
            if self.browse_sdRef in ready[0]:
                pybonjour.DNSServiceProcessResult (self.browse_sdRef)

        for n, s in self.services.iteritems():
            s.stop ()
        for n, s in self.services.iteritems():
            s.join ()
            #print n, 'removed'
            
        self.browse_sdRef.close ()

    def browse_cb (self, browse_sdRef, flags, interfaceIndex, errorCode, 
                   serviceName, regtype, replyDomain):
        if errorCode != pybonjour.kDNSServiceErr_NoError:
            return
        
        if not (flags & pybonjour.kDNSServiceFlagsAdd):
            if not (serviceName == (sd_owner + '.' + sd_name)):
                try:
                    self.services[serviceName].stop ()
                    self.services[serviceName].join ()
                    del self.services[serviceName]
                    #print serviceName, 'removed'
                except:
                    pass
            
        else:
            if not (serviceName == (sd_owner + '.' + sd_name)):
                if not self.services.has_key(serviceName):
                    self.services[serviceName] = SDevice (interfaceIndex,
                                                          serviceName,
                                                          regtype,
                                                          replyDomain)
                    self.services[serviceName].start ()

    def stop (self):
        self.kill = True
        
        
#######################
    
class Connect:
    def __init__ (self, host, port):
        self.host = host
        self.port = port

    def connect (self):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.settimeout (10.0)
        self.s.connect ((self.host,self.port))                

    def setsock (self, s):
        self.s = s

    def readline (self):
        buffer = ""
        data = self.s.recv (1)
        while data is not None:
            if data.startswith ('\n'):
                #print '<< ', buffer
                return buffer
            if data.startswith ('\r'):
                data = self.s.recv (1)
                #print '<< ', buffer
                return buffer
            buffer += str (data)
            data = self.s.recv (1)
        #print '<< ', buffer
        return buffer

    def writeline (self, str):
        #print '>> ', str
        self.s.send (str)
        self.s.send ('\n')

    def close (self):
        self.s.close ()

    def send (self, str):
        self.s.send (str)
    
    def recv (self, len):
        return self.s.recv (len)
    



#######################

class SDevice (Thread):
    def __init__ (self, interfaceIndex, serviceName,
                  regtype, replyDomain):
        Thread.__init__ (self)
        self.interfaceIndex = interfaceIndex
        self.serviceName = serviceName
        self.regtype = regtype
        self.replyDomain = replyDomain
        self.resolved = False
        self.kill = False
        self.threads = []
        self.onlineSent = False

    def resolve_cb (self, sdRef, flags, interfaceIndex, errorCode,
                    fullname, hosttarget, port, txtRecord):
        if errorCode != pybonjour.kDNSServiceErr_NoError:
            return
        self.fullname = fullname
        self.hosttarget = hosttarget
        self.remote_port = port
        
        self.query_sdRef = pybonjour.DNSServiceQueryRecord (interfaceIndex = self.interfaceIndex,
                                                            fullname = self.hosttarget,
                                                            rrtype = pybonjour.kDNSServiceType_A,
                                                            callBack = self.query_cb)
        try:
            while not self.resolved:
                ready = select.select ([self.query_sdRef], [], [], 5)
                if self.query_sdRef not in ready[0]:
                    break
                pybonjour.DNSServiceProcessResult (self.query_sdRef)
                
        finally:
            self.query_sdRef.close ()

    def query_cb (self, sdRef, flags, interfaceIndex, errorCode, 
                  fullname, rrtype, rrclass, rdata, ttl):
        if errorCode == pybonjour.kDNSServiceErr_NoError:
            self.remote_ip = socket.inet_ntoa (rdata)
            self.resolved = True

    def run (self):
        self.resolve_sdRef = pybonjour.DNSServiceResolve (0,
                                                          self.interfaceIndex,
                                                          self.serviceName,
                                                          self.regtype,
                                                          self.replyDomain,
                                                          self.resolve_cb)
        while not self.resolved:
            ready = select.select ([self.resolve_sdRef], [], [], 5)
            if self.resolve_sdRef not in ready[0]:
                break
            pybonjour.DNSServiceProcessResult (self.resolve_sdRef)
            
        self.resolve_sdRef.close ()

        if self.resolved :
            print self.serviceName, ': ', self.remote_ip, ':', self.remote_port        
            self.runLocalSDServer ()

    def runLocalSDServer (self):
        global new_port
        global tun_port
        global pfsd_port
        global threads
        if (new_port == tun_port + 98):
            new_port = tun_port + 1
        self.local_port = new_port
        new_port += 1
    
        tun = Connect (self.remote_ip, self.remote_port)
        try:
            tun.connect ()
            tun.writeline ('WHOIS')
            self.remote_sd_id = tun.readline ()
            self.remote_sd_owner = tun.readline ()
            self.remote_sd_name = tun.readline ()
            tun.close ()
        except:
            self.stop ()
            return

        pfsd = Connect ('localhost', pfsd_port)
        try:
            pfsd.connect ()
            pfsd.writeline ('ONLINE')
            pfsd.writeline ('1')
            pfsd.writeline (str (self.local_port))
            pfsd.writeline (self.remote_sd_id)
            pfsd.writeline (self.remote_sd_owner)
            pfsd.writeline (self.remote_sd_name)
            ans = pfsd.readline ()
            pfsd.writeline ('CLOSE')
            pfsd.close ()
            self.onlineSent = True
        except:
            self.stop ()
        
        try:
            srv_sock = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
            srv_sock.setsockopt (socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            srv_sock.bind (('', self.local_port))
            srv_sock.listen (5)
        except:
            self.stop ()

        #print 'Starting SDServer on port ', self.local_port, ' kill : ', self.kill
        while not self.kill:
            ready = select.select ([srv_sock], [], [], 1)
            if srv_sock in ready[0]:
                srv = SDService (srv_sock.accept (), 
                                 self.remote_ip, 
                                 self.remote_port,
                                 self.remote_sd_id)
                srv.start ()
                self.threads.append (srv)
        
        for srv in self.threads:
            srv.stop ()
        for srv in self.threads:
            srv.join ()
        srv_sock.close ()
        
        
    def stop (self):
        global pfsd_port
        self.kill = True
        pfsd = Connect ('localhost', pfsd_port)
        try:
            if self.onlineSent:
                pfsd.connect ()
                pfsd.writeline ('OFFLINE')
                pfsd.writeline ('1')
                pfsd.writeline (self.remote_sd_id)
                ans = pfsd.readline ()
                pfsd.writeline ('CLOSE')
                pfsd.close ()
        except:
            pass


#######################

class SDService (Thread):
    def __init__ (self, (client, address), ip, port, sd_id):
        Thread.__init__ (self)
        self.client = client
        self.address = address
        self.kill = False
        self.remote_ip = ip
        self.remote_port = port
        self.remote_sd_id = sd_id
        print 'SDService : FROM ', self.address

    def run (self):        
        global pfsd_port
        try:
            try:
                self.tun_sock = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
                self.tun_sock.settimeout (5.0)
                self.tun_sock.connect ((self.remote_ip, self.remote_port))
            except:
                cli = Connect ('', 0)
                cli.setsock (self.client)
                cli.writeline ('FAIL')
                cli.readline ()
                self.stop ()

            if not self.kill:
                tun = Connect ('', 0)
                tun.setsock (self.tun_sock)
                tun.writeline ('CONNECT')
                line = tun.readline ()
                if not line == 'OK':                
                    cli = Connect ('', 0)
                    cli.setsock (self.client)
                    cli.writeline ('FAIL')
                    cli.readline ()
                    self.stop ()
                else:
                    cli = Connect ('', 0)
                    cli.setsock (self.client)
                    cli.writeline ('OK')
                    #print 'Connection to tun ', self.remote_sd_id
                
                    try:
                        while not self.kill:
                            ready_r = select.select ([self.client, self.tun_sock], [], [], 10)
                            if (self.client in ready_r[0]):
                                ready_w = select.select ([], [self.tun_sock], [], 1)
                                if (self.tun_sock in ready_w[1]):
                                    data = self.client.recv (4096)
                                    len = self.tun_sock.send (data)
                                else:
                                    print 'SDService : ERROR 1'
                                    self.stop ()
                            elif (self.tun_sock in ready_r[0]):
                                ready_w = select.select ([], [self.client], [], 1)
                                if (self.client in ready_w[1]):
                                    data = self.tun_sock.recv (4096)
                                    len = self.client.send (data)
                                else:
                                    print 'SDService : ERROR 2'
                                    self.stop ()
                            else:
                                print 'SDService : ERROR 3'
                                self.stop ()
                            if (len == 0):
                                self.stop ()

                    except:
                        pass
        except:
            pass

        self.tun_sock.close ()
        self.client.close ()                
        print 'SDService : CLOSE ', self.address

    def stop (self):
        self.kill = True
    
        



#######################

class TUNServer(Thread):
    def __init__ (self):
        Thread.__init__ (self)
        self.kill = False
        self.threads = []

    def run (self):
        global tun_port
        global sd_id
        global sd_owner
        global sd_name

        try:
            srv_sock = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
            srv_sock.setsockopt (socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            srv_sock.bind (('', tun_port))
            srv_sock.listen (5)
        except:
            self.stop ()

        #print 'Starting TUNServer on port ', tun_port, ' kill : ', self.kill
        while not self.kill:
            ready = select.select ([srv_sock], [], [], 1)
            if srv_sock in ready[0]:
                srv = TUNService (srv_sock.accept ())
                srv.start ()
                self.threads.append (srv)
        
        for srv in self.threads:
            srv.stop ()
        for srv in self.threads:
            srv.join ()
        print 'Closing TUNServer'
        srv_sock.close ()

    def stop (self):
        self.kill = True



#######################

class TUNService (Thread):
    def __init__ (self, (client, address)):
        Thread.__init__ (self)
        self.kill = False
        self.client = client
        self.address = address
        print 'TUNService : FROM ', self.address

    def run (self):
        global sd_id
        global sd_owner
        global sd_name
        global pfsd_port
        cli = Connect ('', 0)
        cli.setsock (self.client)

        try:
            cmd = cli.readline ()
            if cmd == 'WHOIS':
                cli.writeline (sd_id)
                cli.writeline (sd_owner)
                cli.writeline (sd_name)

            if cmd == 'CONNECT':
                try:
                    self.pfsd_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    self.pfsd_sock.settimeout (10.0)
                    self.pfsd_sock.connect (('localhost', pfsd_port))
                except:
                    cli.writeline ('FAIL')
                    self.stop ()
                if (not self.kill):
                    cli.writeline ('OK')
                
                try:
                    while not self.kill:
                        ready_r = select.select ([self.client, self.pfsd_sock], [], [], 10)
                        if (self.client in ready_r[0]):
                            ready_w = select.select ([], [self.pfsd_sock], [], 1)
                            if (self.pfsd_sock in ready_w[1]):
                                data = self.client.recv (4096)
                                len = self.pfsd_sock.send (data)
                            else:
                                print 'TUNService : ERROR 1'
                                self.stop ()
                        elif (self.pfsd_sock in ready_r[0]):
                            ready_w = select.select ([], [self.client], [], 1)
                            if (self.client in ready_w[1]):
                                data = self.pfsd_sock.recv (4096)
                                len = self.client.send (data)
                            else:
                                print 'TUNService : ERROR 2'
                                self.stop ()
                        else:
                            print 'TUNService : ERROR 3'
                            self.stop ()
                        if (len == 0):
                            self.stop ()
                except:
                    pass
                self.pfsd_sock.close ()
        except:
            pass
        self.client.close ()
        print 'TUNService : CLOSE ', self.address

    def stop (self):
        self.kill = True



#######################

if __name__ == "__main__":

    tun_srv_thread = TUNServer ()
    threads.append (tun_srv_thread)
    tun_srv_thread.start ()

    name = sd_owner + '.' + sd_name
    reg_thread = DNSReg (name, '_pfs._tcp', tun_port)
    threads.append (reg_thread)
    reg_thread.start ()
    
    browse_thread = DNSBrowse ('_pfs._tcp')
    threads.append (browse_thread)
    browse_thread.start ()

    try:
        while True:
            time.sleep (1)
            pass
    except KeyboardInterrupt:
        pass

    for t in threads:
        t.stop ()
        t.join ()
    exit ()

