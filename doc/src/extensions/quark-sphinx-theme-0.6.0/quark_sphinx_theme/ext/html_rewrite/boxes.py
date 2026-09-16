from docutils.nodes import admonition, topic, sidebar


class BoxesMixin(object):
    __name_tag__ = 'Boxes'

    def __init__(self, *args, **kwargs):
        pass

def visit_admonition(self, node, name=''):
    self.body.append('<table class="-x-quark-box -x-quark-admonition %s">'
                        '<tbody><tr>'
                        '<td width="100%%" class="-x-quark-box-td">'
                        % ('-x-quark-%s' % name if name else ''))
    type(self).visit_admonition(self, node, name)

def depart_admonition(self, node=None):
    type(self).depart_admonition(self, node)
    self.body.append("</td></tr></tbody></table>")

def visit_topic(self, node):
    self.body.append('<table class="-x-quark-box -x-quark-topic">'
                        '<tbody><tr>'
                        '<td width="100%" class="-x-quark-box-td">')
    type(self).visit_topic(self, node)

def depart_topic(self, node):
    type(self).depart_topic(self, node)
    self.body.append('</td></tr></tbody></table>')

def visit_sidebar(self, node):
    self.body.append('<table class="-x-quark-box -x-quark-sidebar">'
                        '<tbody><tr>'
                        '<td width="35%" class="-x-quark-box-td">')
    type(self).visit_sidebar(self, node)

def depart_sidebar(self, node):
    type(self).depart_sidebar(self, node)
    self.body.append('</td></tr></tbody></table>')


OVERRIDES = [
    (admonition, (visit_admonition, depart_admonition)),
    (topic, (visit_topic, depart_topic)),
    (sidebar, (visit_sidebar, depart_sidebar)),
]
