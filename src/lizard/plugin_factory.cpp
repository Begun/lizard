/* Copyright 2011 ZAO "Begun".
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <dlfcn.h>
#include <lizard/server.hpp>

lizard::plugin_factory::plugin_factory() : loaded_module(0), plugin_handle(0)
{

}

lizard::plugin_factory::~plugin_factory()
{
    unload_module();
}

#ifdef LZ_STATIC
extern "C" lizard::plugin * get_plugin_instance(lizard::server_callback * srv);
#endif

void lizard::plugin_factory::load_module(const lizard::lz_config::ROOT::PLUGIN& pd, const char * config_path, server_callback * srv)
{
    if (0 == pd.easy_threads)
    {
        return;
    }

#ifdef LZ_DLOPEN
    if (0 == loaded_module)
    {
        loaded_module = dlopen(pd.library.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (loaded_module == 0)
        {
            char buff[2048];
            snprintf(buff, 2048, "loading module '%s' failed: %s", pd.library.c_str(), dlerror());

            throw std::logic_error(buff);
        }
    }
    else
    {
        throw std::logic_error("module already loaded!!!");
    }

    dlerror();

    //------------------ UGLY evil hack -----------------
    union conv_union
    {
        void * v;
        lizard::plugin * (*f)(lizard::server_callback*);
    } conv;

    conv.v = dlsym(loaded_module, "get_plugin_instance");
    lizard::plugin * (*func)(lizard::server_callback*) = conv.f;
    //---------------------------------------------------

    const char * errmsg = dlerror();
    if (0 != errmsg)
    {
        char buff[2048];
        snprintf(buff, 2048, "error searching 'get_plugin_instance' in module '%s': '%s'", pd.library.c_str(), errmsg);

        throw std::logic_error(buff);
    }

    plugin_handle = (*func)(srv);
#elif defined(LZ_STATIC)
    plugin_handle = get_plugin_instance(srv);
#endif

    if (0 == plugin_handle)
    {
        char buff[2048];
        snprintf(buff, 2048, "module '%s': instance of plugin is not created", pd.library.c_str());

        throw std::logic_error(buff);
    }

    if (plugin::rSuccess != plugin_handle->set_param(config_path))
    {
        throw std::logic_error("Plugin init failed");
    }

}

void lizard::plugin_factory::unload_module()
{
    if (plugin_handle)
    {
        delete plugin_handle;
        plugin_handle = 0;
    }

    if (loaded_module)
    {
#ifdef LZ_DLOPEN
        dlclose(loaded_module);
#endif
        loaded_module = 0;
    }
}

lizard::plugin * lizard::plugin_factory::get_plugin()const
{
    return plugin_handle;
}

void lizard::plugin_factory::idle()
{
    if (plugin_handle)
    {
        plugin_handle->idle();
    }
}
