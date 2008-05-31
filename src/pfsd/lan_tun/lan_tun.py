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
timeout = 5
new_port = tun_port + 1

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
            s.join ()
            print n, 'removed'
            
        self.browse_sdRef.close ()

    def browse_cb (self, browse_sdRef, flags, interfaceIndex, errorCode, 
                   serviceName, regtype, replyDomain):
        if errorCode != pybonjour.kDNSServiceErr_NoError:
            return
        
        if not (flags & pybonjour.kDNSServiceFlagsAdd):
            if not (serviceName == (sd_owner + '.' + sd_name)):
                self.services[serviceName].stop ()
                self.services[serviceName].join ()
                del self.services[serviceName]
                print serviceName, 'removed'
            
        else:
            if not (serviceName == (sd_owner + '.' + sd_name)):
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
        self.s.connect((self.host,self.port))                

    def readline (self):
        buffer = ""
        data = self.s.recv (1)
        while data is not None:
            if data.startswith ('\n'):
                return buffer
            buffer += str (data)
            data = self.s.recv (1)
        return buffer

    def writeline (self, str):
        self.s.send (str)
        self.s.send ('\n')

    def close (self):
        print 'closing conn'
        self.s.close ()



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
                ready = select.select ([self.query_sdRef], [], [], timeout)
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
            ready = select.select ([self.resolve_sdRef], [], [], timeout)
            if self.resolve_sdRef not in ready[0]:
                break
            pybonjour.DNSServiceProcessResult (self.resolve_sdRef)
            
        self.resolve_sdRef.close ()

        if self.resolved :
            print self.serviceName, ': ', self.remote_ip, ':', self.remote_port        
            self.runLocalSDServer ()

    def runLocalSDServer (self):
        global new_port
        if (new_port == tun_port + 98):
            new_port = tun_port + 1
        self.local_port = new_port
        new_port += 1
    
        
        
        
    def stop (self):
        self.kill = True
            


#######################

class TUNServer(Thread):
    def __init__ (self, port):
        Thread.__init__ (self)
        self.port = port
        
    

#######################

if __name__ == "__main__":

    name = sd_owner + '.' + sd_name
    reg_thread = DNSReg (name, '_pfs._tcp', tun_port)
    reg_thread.start ()
    browse_thread = DNSBrowse ('_pfs._tcp')
    browse_thread.start ()

    try:
        while True:
            time.sleep (1)
    except KeyboardInterrupt:
        pass

    reg_thread.stop ()
    browse_thread.stop ()

