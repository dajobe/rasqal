/* Rasqal configure wrapper for libsv
 *
 * Includes configuration file
 * Adjusts symbols to be all rasqal_ prefixed
 */

#include <rasqal_config.h>

#define sv_init rasqal_sv_init
#define sv_free rasqal_sv_free
#define sv_get_line rasqal_sv_get_line
#define sv_get_header rasqal_sv_get_header
#define sv_parse_chunk rasqal_sv_parse_chunk

