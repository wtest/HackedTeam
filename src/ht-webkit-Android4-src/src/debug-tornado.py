import logging
from tornado.wsgi import WSGIContainer
from tornado.ioloop import IOLoop
from tornado.web import FallbackHandler, RequestHandler, Application
from debugserver import app

class MainHandler(RequestHandler):
    def get(self):
        self.write("Tornado server OK")

accesslog = logging.getLogger("tornado.access")
accesslog.setLevel(logging.INFO)
tr = WSGIContainer(app)

application = Application([
    (r"/tornado", MainHandler),
    (r".*", FallbackHandler, dict(fallback=tr)),
], debug=True)

if __name__ == "__main__":
    application.listen(8080)
    IOLoop.instance().start()
