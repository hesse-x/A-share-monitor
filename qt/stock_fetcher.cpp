#include "stock_fetcher.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <string>

// Callback function: Save response data
size_t StockFetcher::writeCallback(void *contents, size_t size, size_t nmemb,
                                   std::string *s) {
  size_t newLength = size * nmemb;
  try {
    s->append((char *)contents, newLength);
  } catch (const std::bad_alloc &e) {
    return 0;
  }
  return newLength;
}

// String splitting utility
std::vector<std::string> StockFetcher::split(const std::string &s,
                                             char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

// Constructor: Initialize stock code
StockFetcher::StockFetcher(std::string stockCode)
    : m_stockCode(std::move(stockCode)) {}

// Core method: Fetch data once and return fields 1-7
std::vector<float> StockFetcher::fetchData() {
  std::vector<float> result;
  std::string response_data;
  CURL *curl = nullptr;
  CURLcode res;

  // Initialize curl
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  if (!curl) {
    std::cerr << "curl initialization failed" << std::endl;
    return result;
  }

  // Set request URL
  std::string url = "http://hq.sinajs.cn/list=" + m_stockCode;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  // Set request headers (simulate browser)
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Referer: https://finance.sina.com.cn/");
  headers = curl_slist_append(
      headers,
      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  // Set response data callback
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

  // Execute request
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
  } else {
    // Parse response data
    size_t start = response_data.find('"');
    size_t end = response_data.find('"', start + 1);
    if (start != std::string::npos && end != std::string::npos) {
      std::string stock_data = response_data.substr(start + 1, end - start - 1);
      std::vector<std::string> fields = split(stock_data, ',');

      // Extract fields 1-7 (indices 1 to 7, total 7 fields)
      if (fields.size() >= 8) { // Ensure at least 8 fields (0-7)
        for (int i = 1; i <= 7; ++i) {
          try {
            float value = std::stof(fields[i]); // Convert to float
            result.push_back(value);
          } catch (const std::exception &e) {
            result.push_back(0.0f); // Fill 0 on conversion failure
          }
        }
      } else {
        std::cerr << "Data format error, insufficient fields" << std::endl;
      }
    } else {
      std::cerr << "Failed to parse response data" << std::endl;
    }
  }

  // Clean up resources
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();

  return result;
}
