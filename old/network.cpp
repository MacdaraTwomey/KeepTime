#include "string.h"
#include <curl/curl.h>

bool request_favicon_from_website(String url, Size_Mem *icon_file);

Bitmap
get_favicon_from_website(String url)
{
    // Url must be null terminated
    
    // mit very screwed
    // stack overflow, google has extra row on top?
    // onesearch.library.uwa.edu.au is 8 bit
    // teaching.csse.uwa.edu.au hangs forever, maybe because not SSL?
    // https://www.scotthyoung.com/ 8 bit bitmap, seems fully transparent, maybe aren't using AND mask right? FAIL
    // http://ukulelehunt.com/ not SSL, getting Success read 0 bytes
    // forum.ukuleleunderground.com 15x16 icon, seemed to render fine though...
    // onesearch has a biSizeImage = 0, and bitCount = 8, and did set the biClrUsed field to 256, when icons shouldn't
    // voidtools.com
    // getmusicbee.com
    
    // This is the site, must have /en-US on end
    // https://www.mozilla.org/en-US/ 8bpp seems the and mask is empty the way i'm trying to read it
    
    // lots of smaller websites dont have an icon under /favicon.ico, e.g. ukulele sites
    
    // maths doesn't seem to work out calcing and_mask size ourmachinery.com
    
    // https://craftinginterpreters.com/ is in BGR format just like youtube but stb image doesn't detect
    
    
    // TODO: Validate url and differentiate from user just typing in address bar
    
    
    //String url = make_string_from_literal("https://craftinginterpreters.com/");
    
    
    Bitmap favicon = {};
    Size_Mem icon_file = {};
    bool request_success = request_favicon_from_website(url, &icon_file);
    if (request_success)
    {
        tprint("Request success");
        favicon = decode_favicon_file(icon_file);
        free(icon_file.memory);
    }
    else
    {
        tprint("Request failure");
    }
    
    if (!favicon.pixels)
    {
        favicon = make_bitmap(10, 10, RGBA(190, 25, 255, 255));
    }
    
    return favicon;
}


String
search_html_for_icon_url(String html_page)
{
    // SCOPUS has rel="SHORTCUT ICON" ???
    constexpr String rel_attributes_test[] = {
        make_const_string("icon"),
        make_const_string("shortcut icon"),
    };
    static_assert(rel_attributes_test[0].str[0] == 'i', "err");
    
    char * rel_attributes[] = {
        "icon",
        "shortcut icon",
    };
    
    for (int i = 0; i < array_count(rel_attributes); ++i)
    {
        // We search entire page for each rel_attribute because we expect to likely find in first or second
        // attempt and don't want to iterate whole rel_attribute array for each found link.
        
        // TODO: Maybe make returns past end of html_page length if not found, so dont need check here, because it is handled in the next call
        i32 start = 0;
        for (;;)
        {
            i32 at = search_for_substr(html_page, start, "link");
            if (at == -1) break;
            
            if (at != 0) at -= 1;
            for (; at >= 0 && html_page.str[at] == ' '; --at);
            if (html_page.str[at] != '<') break;
            
            i32 end = search_for_char(html_page, at, '>');
            if (end == -1) break;
            
            // Resume here if don't find what we're looking for in tag
            start = end + 1;
            
            String link = substr_range(html_page, at, end);
            
            auto get_attribute_value = [&](String link, char *attribute_name, String *value) -> bool {
                i32 attr = search_for_substr(link, 0, attribute_name);
                if (attr == -1) return false;
                
                // Assume next set of quotes is for this attribute
                i32 quote_start = search_for_char(link, attr, '"');
                if (quote_start == -1) return false;
                
                i32 quote_end = search_for_char(link, quote_start + 1, '"');
                if (quote_end == -1) return false;
                
                *value = substr_range(link, quote_start+1, quote_end-1);
                return true;
            };
            
            String rel_attribute_val;
            if (!get_attribute_value(link, "rel", &rel_attribute_val)) continue;
            
            // TODO: Maybe do loop over this here.
            if (!string_equals(rel_attribute_val, rel_attributes[i])) continue;
            
            String href_attribute_val;
            if (!get_attribute_value(link, "href", &href_attribute_val)) continue;
            
            if (href_attribute_val.length > 0)
            {
                return href_attribute_val;
            }
        }
    }
    
    String empty_string = {};
    return empty_string;
}


void
curl_print_err(CURLcode code, char *errbuf)
{
    size_t len = strlen(errbuf);
    fprintf(stderr, "\nlibcurl: (%d) ", code);
    if(len)
        fprintf(stderr, "%s%s", errbuf,
                ((errbuf[len - 1] != '\n') ? "\n" : ""));
    else
        fprintf(stderr, "%s\n", curl_easy_strerror(code));
}

size_t
recieve_data_callback(void *buffer, size_t size, size_t nmemb, void *userp)
{
    Size_Mem *recieved_data = (Size_Mem *)userp;
    memcpy(recieved_data->memory + recieved_data->size, buffer, nmemb);
    recieved_data->size += nmemb;
    return nmemb;
}

bool
extract_scheme_and_host(String full_url, String *url_host)
{
    bool set_url_success = false;
    CURLU *curl_full_url_handle = curl_url();
    CURLU *new_url_handle = curl_url();
    if (curl_full_url_handle && new_url_handle)
    {
        // If there is no scheme i think this fails
        CURLUcode result = curl_url_set(curl_full_url_handle, CURLUPART_URL, full_url.str, 0);
        if (result == CURLE_OK)
        {
            bool set_scheme = false;
            bool set_host = false;
            char *scheme;
            CURLUcode result = curl_url_get(curl_full_url_handle, CURLUPART_SCHEME, &scheme, 0);
            if(result == CURLE_OK)
            {
                result = curl_url_set(new_url_handle, CURLUPART_SCHEME, scheme, 0);
                curl_free(scheme);
                if (result == CURLE_OK) set_scheme = true;
            }
            
            char *hostname;
            result = curl_url_get(curl_full_url_handle, CURLUPART_HOST , &hostname, 0);
            if(result == CURLE_OK)
            {
                result = curl_url_set(new_url_handle, CURLUPART_HOST, hostname, 0);
                curl_free(hostname);
                if (result == CURLE_OK) set_host = true;
            }
            
            char *new_url;
            result = curl_url_get(new_url_handle, CURLUPART_URL, &new_url, 0);
            if(result == CURLE_OK)
            {
                // We dont really care if scheme got set or not for now
                if (set_host)
                {
                    set_url_success = true;
                    append_string(url_host, new_url);
                    curl_free(new_url);
                }
            }
        }
    }
    
    curl_url_cleanup(curl_full_url_handle);
    curl_url_cleanup(new_url_handle);
    
    null_terminate(url_host);
    
    return set_url_success;
}

bool
get_icon_from_google_s2(CURL *handle, String url_host, Size_Mem *icon_file)
{
    char buf[2000];
    String google_s2_url = make_empty_string(buf);
    append_string(&google_s2_url, "http://s2.googleusercontent.com/s2/favicons?domain_url=");
    append_string(&google_s2_url, url_host);
    null_terminate(&google_s2_url);
    
    curl_easy_setopt(handle, CURLOPT_URL, google_s2_url.str);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, recieve_data_callback);
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    
    *icon_file = {};
    icon_file->memory = (u8 *)xalloc(Megabytes(8));
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)icon_file);
    
    if (curl_easy_perform(handle) != CURLE_OK)
    {
        free(icon_file->memory);
        icon_file->memory = nullptr;
        icon_file->size = 0;
        return false;
    }
    
    return true;
}

bool
get_icon_location_from_main_page(CURL *handle, String url_host, String *icon_location, Size_Mem *response)
{
    Assert(response->size > 0);
    Assert(response->memory);
    // NOTE: Must be curl init etc ...
    bool success = false;
    
    // must be scheme://host:port/path format
    curl_easy_setopt(handle, CURLOPT_URL, url_host.str);
    
    // Usually called by libcurl multiple times
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, recieve_data_callback);
    
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)response);
    
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
    
    /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    
    if (curl_easy_perform(handle) == CURLE_OK)
    {
        response->memory[response->size] = '\0';
        String html_page = make_string(response->memory, response->size);
        
        String linked_icon_url = search_html_for_icon_url(html_page);
        if (linked_icon_url.length > 0)
        {
            linked_icon_url.str[linked_icon_url.length] = '\0'; // overwriting html_page
            
            CURLU *h = curl_url();
            if (h)
            {
                // Changes whole url if linked_icon_url is an absolute url or just appends if it is relative
                // TODO: Test if this is not prefixed with a slash
                CURLUcode result = curl_url_set(h, CURLUPART_URL, linked_icon_url.str, 0);
                if(result == CURLE_OK)
                {
                    char *full_url;
                    result = curl_url_get(h, CURLUPART_URL, &full_url, 0);
                    if(result == CURLE_OK)
                    {
                        append_string(icon_location, full_url);
                        null_terminate(icon_location);
                        
                        curl_free(full_url);
                        success = true;
                    }
                    else
                    {
                        tprint("Network error: Could not create absolute url");
                    }
                }
                else
                {
                    tprint("Network error: The icon url we recieved from html page was invalid");
                }
                
                curl_url_cleanup(h);
            }
        }
        else
        {
            tprint("Couldnt find favicon in help page");
        }
    }
    
    return success;
}

bool
request_favicon_from_website(String url, Size_Mem *icon_file)
{
    // url must be null terminted
    // url must have no path
    
    // https://blog.hubspot.com/marketing/parts-url
    // protocol:         https://    always 'there' not always visible
    // subdomain:        blog        not required
    // domain name:      hubspot     always there (note aka second-level domain, it is level below TLD)
    // top level domain: .com        required
    // path:             /marketing/parts-url  always 'there' not always visible (browsers rewrite url)
    
    // Hostname: blog.hubspot.com
    
    // NOTE: TODO: For now we remove the path from the url passed in, in the future, this function will take urls with path already removed
#define print_error(code) curl_print_err((code), errbuf);
    
    Assert(string_is_null_terminated(url));
    
    bool success = false;
    
    char builder_buf[2000];
    String url_host = make_empty_string(builder_buf);
    
    bool extracted = extract_scheme_and_host(url, &url_host);
    if (!extracted)
    {
        tprint("Could not get host part from url");
        return false;
    }
    
    Assert(string_is_null_terminated(url_host));
    
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    CURL *handle = curl_easy_init();
    if(!handle) return false;
    
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errbuf);
    
    bool get_icon = get_icon_from_google_s2(handle, url_host, icon_file);
    if (get_icon)
    {
        success = true;
    }
    
#if 0
    
    
    // TODO: Realloc and grow if size > initial alloc + make first alloc smaller
    Size_Mem response = {};
    response.memory = (u8 *)xalloc(Megabytes(8));
    
    char buf[2000];
    String icon_location = make_empty_string(buf);
    if (get_icon_location_from_main_page(handle, url_host, &icon_location) == false)
    {
        // Default
        append_string(&icon_location, url_host);
        append_string(&icon_location, "/favicon.ico");
    }
    
    
    
    // Do another GET
    curl_easy_setopt(handle, CURLOPT_URL, absolute_icon_url.str);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, recieve_data_callback);
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    
    // Write to icon file memory
    // TODO: Realloc in writeback callback
    icon_file->memory = (u8 *)xalloc(Megabytes(8));
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)icon_file);
    
    bool success = (curl_easy_perform(handle) == CURLE_OK);
    if (!success)
    {
        free(icon_file->memory);
        icon_file->memory = nullptr;
        icon_file->size = 0;
    }
    free(response.memory);
    
#endif
    
    
    curl_easy_cleanup(handle);
    
    curl_global_cleanup(); // global_init and global_cleanup should only really be called once by program.
    
    return success;
}