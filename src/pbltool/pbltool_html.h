/* pbltool_html.h
 * Helpers for digesting the HTML template.
 */
 
#ifndef PBLTOOL_HTML_H
#define PBLTOOL_HTML_H

struct sr_encoder;

#define PBLTOOL_HTML_NODETYPE_TEXT 1
#define PBLTOOL_HTML_NODETYPE_SPACE 2 /* Technically TEXT but you'll probably want to ignore it. */
#define PBLTOOL_HTML_NODETYPE_COMMENT 3
#define PBLTOOL_HTML_NODETYPE_DOCTYPE 4
#define PBLTOOL_HTML_NODETYPE_SINGLETON 5
#define PBLTOOL_HTML_NODETYPE_OPEN 6
#define PBLTOOL_HTML_NODETYPE_CLOSE 7

struct pbltool_html_decoder {
  const char *src;
  int srcc;
  int srcp;
  int nextc;
  int nodetype;
  const char *path;
  int lineno;
};

/* pbltool_html_decoder is carefully designed to not require cleanup.
 * Initializing never fails.
 */
void pbltool_html_decoder_init(struct pbltool_html_decoder *decoder,const char *src,int srcc,const char *path);

/* Return the next chunk of text from the HTML file.
 * Zero at end of file, and errors are possible.
 * After a success, (nodetype) and (lineno) both refer to the returned token.
 */
int pbltool_html_decoder_next(void *dstpp,struct pbltool_html_decoder *decoder);

/* Only legal if we are pointed at an OPEN node.
 * Consumes everything thru the corresponding CLOSE node.
 * (dstpp) is optional; if present, we return everything between the OPEN and CLOSE nodes, exclusive.
 * We impose a private depth limit. Heavily nested HTML will fail.
 */
int pbltool_html_decoder_skip_tag(void *dstpp,struct pbltool_html_decoder *decoder);

/* Components of SINGLETON and OPEN nodes.
 * Tag name is also valid for CLOSE.
 * These never return errors but may be zero.
 * Attribute keys match case-sensitively, keep them lower-case. (that's a deliberate violation of HTML spec).
 */
int pbltool_html_tag_name(void *dstpp,const char *src,int srcc);
int pbltool_html_attribute(void *dstpp,const char *src,int srcc,const char *k,int kc);

/* (src) is plain text; escape all '<' and '&'.
 */
int pbltool_html_encode_text(struct sr_encoder *dst,const char *src,int srcc);

/* Measure a chunk of text. One of:
 *  - Whitespace and comments.
 *  - String literal.
 *  - Regex literal immediately prefixed by '(' -- otherwise we can't distinguish from the division operator.
 *  - Import statements, full line.
 *  - Other tokens clumped together with no space.
 * It's very opinionated, and not even entirely compliant with the spec.
 * But the idea is we give you chunks of text that either get dropped or included verbatim.
 */
int pbltool_js_measure(const char *src,int srcc);

int pbltool_html_count_newlines(const char *src,int srcc);

#endif
