from twisted.web.resource import Resource
from twisted.web import server, resource
from twisted.web.server import NOT_DONE_YET
import twisted
from twisted.internet import protocol, reactor

f = open("html_content.html")


htmlData=f.read()
f.close()

teststr=""
connections=[]

class Echo(protocol.Protocol):
	def dataReceived(self,data):
		global teststr
		global connections
		print(data)
		teststr1=data.rstrip("\0")
		for j in teststr1:
			teststr = j	
			for i in connections:
				cycle(i.write)

class EchoFactory(protocol.Factory):
	def buildProtocol(self, addr):
		return Echo()


def cycle(echo):
	global teststr
	echo("event: userlogon\n")
	print("CYCLE"+teststr)
#	echo('data: {"username":"star"}')
	echo('data: {"username":"' + teststr + '"}')
	echo("\n\n")


class MyResource2(Resource):
	isLeaf = True
	def render_GET(self, request):
		global connections
		if request.postpath.pop(0)!='':
			request.setHeader("Content-Type", "text/event-stream")
			request.setHeader("Cache-Control", "no-cache")
			request.setHeader("Connection", "keep-alive")
			connections.append(request)		
#			lc = twisted.internet.task.LoopingCall(cycle,request.write)
			#lc.start(1)
			#request.notifyFinish().addBoth(lambda _: lc.stop())
			return NOT_DONE_YET
		return htmlData
		
root=MyResource2()
site = server.Site(root)
reactor.listenTCP(8889,EchoFactory())
reactor.listenTCP(8080,site)
reactor.run()
