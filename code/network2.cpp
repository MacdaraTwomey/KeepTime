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
    static_assert(rel_attributes[0].str[0] == 'r', "err");
    
    char * rel_attributes[] = {
        "icon",
        "shortcut icon",
    };
    
    String icon_url = {};
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
            
            for (; at >= 0 && html_page[at] == ' '; --at);
            if (html_page.str[at] != '<') break;
            
            i32 end = search_for_char(html_page, at, '>');
            if (end == -1) break;
            
            // Resume here if don't find what we're looking for in tag
            start = end + 1;
            
            {
                String link = make_substr_range(html_page, at, end);
                i32 rel = search_for_str(link, 0, "rel");
                if (rel == -1) continue;
                
                i32 quote_start = search_for_char(link, rel, '"');
                if (quote_start == -1) continue;
                
                i32 quote_end = search_for_char(link, quote_start + 1, '"');
                if (quote_end == -1) continue;
                
                // TODO: Out of interest does compiler auto deduce char * length inside call, could also test with literal "icon".
                String val = make_string_range(link, quote_start+1, quote_end-1);
                if (!string.equals(val, rel_attribute[i])) continue;
            }
            
            
            i32 href = search_for_str(link, 0, "href");
            if (href == -1) continue;
            
            i32 quote_start = search_for_char(link, href, '"');
            if (quote_start == -1) continue;
            
            i32 quote_end = search_for_char(link, quote_start + 1, '"');
            if (quote_end == -1) continue;
            
            icon_url = make_string_range(link, quote_start+1, quote_end-1);
            return icon_url;
        }
    }
    
    return icon_url;
}


struct Size_Data
{
    u8 *data;
    size_t size;
};

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
    Size_Data *recieved_data = (Size_Data *)userp;
    memcpy(recieved_data->data + recieved_data->size, buffer, nmemb);
    recieved_data->size += nmemb;
    return nmemb;
}

bool
request_icon_from_url(String url, Size_Data *icon_file)
{
    // url must be null terminted
    // url must have no path
    
    
#define print_error(code) curl_print_err((code), errbuf);
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    CURL *handle = curl_easy_init();
    if(!handle) return false;
    
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errbuf);
    
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
    
    // must be scheme://host:port/path format
    curl_easy_setopt(handle, CURLOPT_URL, url.str);
    
    // TODO: Realloc and grow if size > initial alloc + make first alloc smaller
    Size_Data response = {};
    response.data = (u8 *)xalloc(MEGABYTES(8));
    
    // Usually called multiple times
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, recieve_data_callback);
    
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&response);
    
    /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    
    CURLcode result = curl_easy_perform(handle);
    if (result != CURLE_OK) print_error(result);
    
    tprint((char *)response.data);
    
    tprint("SUCCESS");
    
    response.data[response.size] = '\0';
    String html_page = make_string(response.data, response.size);
    
    String parsed_icon_url = search_html_for_icon_url(html_page);
    
    char buf[2000];
    String absolute_icon_url = make_empty_string(buf);
    
    if (icon_url.length > 0)
    {
        icon_url.str[icon_url.length] = '\0'; // overwriting html_page
        
        CURLU *url = curl_url();
        if (url)
        {
            CURLUcode result = curl_url_set(url, CURLUPART_URL, icon_url.str, 0);
            if(result == CURLE_OK) {
                char *host;
                result = curl_url_get(url, CURLUPART_HOST, &host, 0);
                if(result == CURLE_OK) {
                    printf("Host name: %s\n", host);
                    curl_free(host);
                }
                else
                {
                    // No host, so probably relative url
                    char *path;
                    result = curl_url_get(h, CURLUPART_PATH, &path, 0);
                    if(result == CURLE_OK) {
                        printf("Path: %s\n", path);
                        curl_free(path);
                    }
                }
                
                
            }
            
            curl_url_cleanup(url);
        }
    }
    else
    {
        // Default
        icon_url = make_string_from_literal("/favicon.ico");
    }
    
    curl_easy_cleanup(handle);
    
    free(response.data);
    
    curl_global_cleanup(); // global_init and global_cleanup should only really be called once by program.
    
    return true;
}