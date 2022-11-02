#include "cmark-gfm-core-extensions.h"
#include "autolink.h"
#include "mutex.h"
#include "strikethrough.h"
#include "table.h"
#include "tagfilter.h"
#include "tasklist.h"
#include "registry.h"
#include "plugin.h"

static int core_extensions_registration(cmark_plugin *plugin) {
  cmark_plugin_register_syntax_extension(plugin, create_table_extension());
  cmark_plugin_register_syntax_extension(plugin,
                                         create_strikethrough_extension());
  cmark_plugin_register_syntax_extension(plugin, create_autolink_extension());
  cmark_plugin_register_syntax_extension(plugin, create_tagfilter_extension());
  cmark_plugin_register_syntax_extension(plugin, create_tasklist_extension());
  return 1;
}

CMARK_DEFINE_ONCE(registered);

static void register_plugins(void) {
  cmark_register_plugin(core_extensions_registration);
}

void cmark_gfm_core_extensions_ensure_registered(void) {
  CMARK_RUN_ONCE(registered, register_plugins);
}
