from docutils.nodes import literal_block, SkipNode


def append_epilog(body):
    body.append('</td></tr></tbody></table>')

def visit_literal_block(self, node):
    self.body.append('<table class="-x-quark-literal-block">'
                        '<tbody><tr>'
                        '<td width="100%" class="-x-quark-literal-block-td">')
    try:
        type(self).visit_literal_block(self, node)
    except SkipNode:
        append_epilog(self.body)
        raise

def depart_literal_block(self, node):
    type(self).depart_literal_block(self, node)
    append_epilog(self.body)

OVERRIDES = [
    (literal_block, (visit_literal_block, depart_literal_block)),
]
