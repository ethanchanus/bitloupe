from docutils.nodes import citation, footnote


def visit_citation(self, node):
    no_id_node = node.copy()
    no_id_node.delattr('ids')
    self.body.append('<div id="%s" class="-x-quark-citation-wrapper">'
                        % node.get('ids')[0])
    type(self).visit_citation(self, no_id_node)

def depart_citation(self, node):
    type(self).depart_citation(self, node)
    self.body.append('</div>')

def visit_footnote(self, node):
    no_id_node = node.copy()
    no_id_node.delattr('ids')
    self.body.append('<div id="%s" class="-x-quark-footnote-wrapper">'
                        % node.get('ids')[0])
    type(self).visit_footnote(self, no_id_node)

def depart_footnote(self, node):
    type(self).depart_footnote(self, node)
    self.body.append('</div>')

OVERRIDES = [
    (citation, (visit_citation, depart_citation)),
    (footnote, (visit_footnote, depart_footnote)),
]
