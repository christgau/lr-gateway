from twisted.internet import reactor, protocol, task, stdio
from twisted.protocols import basic
from random import randint

class MyProtocol(protocol.Protocol):
    def connectionMade(self):
        self.factory.clientConnectionMade(self)
    def connectionLost(self, reason):
        self.factory.clientConnectionLost(self)

class MyFactory(protocol.Factory):
    protocol = MyProtocol
    def __init__(self):
        self.clients = []
        self.lc = task.LoopingCall(self.announce)
        self.lc.start(10)

    def announce(self):
        pass
#        for client in self.clients:
#            client.transport.write("10 seconds has passed\n")

    def clientConnectionMade(self, client):
        self.clients.append(client)

    def clientConnectionLost(self, client):
        self.clients.remove(client)

    def broadcast(self, msg):
        for client in self.clients:
            self.write(client, msg);

    def write(self, client, msg):
        client.transport.write(chr(2) + msg + chr(3))


class Echo(basic.LineReceiver):
    from os import linesep as delimiter

    def __init__(self, feedproto):
        self.feedproto = feedproto

    def connectionMade(self):
        self.transport.write('>>> ')

    def lineReceived(self, line):
        self.sendLine('Echo: ' + line)
        items = line.split()
        if len(items) > 0:
            cmd = items[0]
            if cmd == "raw":
                remainder = line[len(cmd):].trim()
                self.feedproto.broadcast(remainder)
            elif cmd == "stt":
                comp = items[1]
                state = items[2]
                self.feedproto.broadcast('|'.join(["STT0", comp, comp, state]))
            elif cmd == "logr":
                comp = items[1]
                boat = items[2]
                split = items[3]
                self.feedproto.broadcast('|'.join(["LOG0", comp, comp, boat, 'y', split,
                    str(randint(0,7)) + ':' + str(randint(0,59)) + '.' + str(randint(0, 99)),
                    str(randint(1, 6)),
                    '+' + str(randint(0, 59)) + '.' + str(randint(0, 99))
                    ]))

        self.transport.write('>>> ')

def main():
    myfactory = MyFactory()
    stdio.StandardIO(Echo(myfactory))
    reactor.listenTCP(9000, myfactory)
    reactor.run()

if __name__ == '__main__':
    main()
