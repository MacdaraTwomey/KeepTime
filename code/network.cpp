#include "string.h"
#include <curl/curl.h>


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
request_favicon_from_website(String url, Size_Mem *icon_file)
{
    // url must be null terminted
    // url must have no path
    
#define print_error(code) curl_print_err((code), errbuf);
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    CURL *handle = curl_easy_init();
    if(!handle) return false;
    
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errbuf);
    
    // must be scheme://host:port/path format
    curl_easy_setopt(handle, CURLOPT_URL, url.str);
    
    // TODO: Realloc and grow if size > initial alloc + make first alloc smaller
    Size_Mem response = {};
    response.memory = (u8 *)xalloc(MEGABYTES(8));
    
    // Usually called by libcurl multiple times
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, recieve_data_callback);
    
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&response);
    
    
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
    
    /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    
    if (curl_easy_perform(handle) == CURLE_OK)
    {
        tprint("HTML GET SUCCESS");
    }
    
    response.memory[response.size] = '\0';
    String html_page = make_string(response.memory, response.size);
    
    String linked_icon_url = search_html_for_icon_url(html_page);
    
    char buf[2000];
    String absolute_icon_url = make_empty_string(buf);
    
    bool got_url = false;
    if (linked_icon_url.length > 0)
    {
        linked_icon_url.str[linked_icon_url.length] = '\0'; // overwriting html_page
        
        CURLU *h = curl_url();
        if (h)
        {
            // Set the url to what was passed in. e.g. https://youtube.com
            CURLUcode result = curl_url_set(h, CURLUPART_URL, url.str, 0);
            if (result == CURLE_OK)
            {
                // Changes whole url if linked_icon_url is an absolute url or just appends if it is relative
                // TODO: Test if this is not prefixed with a slash
                result = curl_url_set(h, CURLUPART_URL, linked_icon_url.str, 0);
                if(result == CURLE_OK)
                {
                    char *full_url;
                    result= curl_url_get(h, CURLUPART_URL, &full_url, 0);
                    if(result == CURLE_OK)
                    {
                        got_url = true;
                        append_string(&absolute_icon_url, full_url);
                        curl_free(full_url);
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
            }
            else
            {
                // This should always succeed if we actually managed to get response message from site.
                tprint("Network error: Invalid passed url");
                Assert(0);
            }
            
            curl_url_cleanup(h);
        }
    }
    
    if (!got_url)
    {
        // Default
        append_string(&absolute_icon_url, url);
        append_string(&absolute_icon_url, "/favicon.ico");
    }
    
    
    bool is_null_terminated = null_terminate(&absolute_icon_url);
    Assert(is_null_terminated);
    
    
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
    
    curl_easy_cleanup(handle);
    
    free(response.memory);
    
    curl_global_cleanup(); // global_init and global_cleanup should only really be called once by program.
    
    return success;
}