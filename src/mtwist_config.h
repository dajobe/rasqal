/* Rasqal configure wrapper for libmtwist
 *
 * Includes configuration file
 * Adjusts symbols to be all rasqal_ prefixed
 */

#include <rasqal_config.h>

#define mtwist_new rasqal_mtwist_new
#define mtwist_free rasqal_mtwist_free
#define mtwist_init rasqal_mtwist_init
#define mtwist_u32rand rasqal_mtwist_u32rand
#define mtwist_drand rasqal_mtwist_drand
#define mtwist_seed_from_system rasqal_mtwist_seed_from_system
