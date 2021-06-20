#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <execution>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {   
public:
    using FindResult = std::vector<Document>;
    using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    using ConstIterator = std::set<int>::const_iterator;
    using MapWordFreqs = std::map<std::string_view, double>;
    
    template <typename StringContainer>
    explicit                                            SearchServer(const StringContainer& stop_words);
    
    explicit                                            SearchServer(const std::string& stop_words_text);   
    
    explicit                                            SearchServer(const std::string_view stop_words_text);

    void                                                AddDocument(int document_id,
                                                                    const std::string_view document,
                                                                    DocumentStatus status,
                                                                    const std::vector<int>& ratings); 

    template <typename DocumentPredicate>
    FindResult                                          FindTopDocuments(const std::string_view raw_query,
                                                                         DocumentPredicate document_predicate) const; 

    FindResult                                          FindTopDocuments(const std::string_view raw_query,
                                                                         DocumentStatus status) const; 

    FindResult                                          FindTopDocuments(const std::string_view raw_query) const; 
    
    template <typename DocumentPredicate,
              typename Policy>
    FindResult                                          FindTopDocuments(Policy& policy,
                                                                         const std::string_view raw_query,
                                                                         DocumentPredicate document_predicate) const; 

    template <typename Policy>
    FindResult                                          FindTopDocuments(Policy& policy,
                                                                         const std::string_view raw_query,
                                                                         DocumentStatus status) const; 

    template <typename Policy>
    FindResult                                          FindTopDocuments(Policy& policy,
                                                                         const std::string_view raw_query) const; 
    
    int                                                 GetDocumentCount() const; 
    
    MatchDocumentResult                                 MatchDocument(const std::string_view raw_query,
                                                                      int document_id) const; 
    
    template <typename Policy>
    MatchDocumentResult                                 MatchDocument(Policy& policy,
                                                                      const std::string_view raw_query,
                                                                      int document_id) const; 
    
    ConstIterator                                       begin() const; 
    
    ConstIterator                                       end() const;
    
    const MapWordFreqs&                                 GetWordFrequencies(int document_id) const; 
    
    void                                                RemoveDocument(int document_id);
    
    template <typename Policy>
    void                                                RemoveDocument(Policy& policy,
                                                                       int document_id); 
    
private:
    struct DocumentData {
        int                 rating;
        DocumentStatus      status;
        std::string         data;
        MapWordFreqs        word_to_freqs; 
    };
   
private:
    struct QueryWord {
        std::string_view    data;
        bool                is_minus;
        bool                is_stop;
    };
    
private:
    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };
    
private:
    const SetString                                     stop_words_;
    std::map<std::string_view, std::map<int, double>>   word_to_document_freqs_;
    std::map<int, DocumentData>                         documents_;
    std::set<int>                                       document_ids_;

private:
    bool                                                IsStopWord(const std::string_view word) const; 

    bool static                                         IsValidWord(const std::string_view word); 

    std::vector<std::string_view>                       SplitIntoWordsNoStop(const std::string_view text) const; 

    static int                                          ComputeAverageRating(const std::vector<int>& ratings); 

    QueryWord                                           ParseQueryWord(std::string_view text) const; 

    Query                                               ParseQuery(const std::string_view text) const; 

    double                                              ComputeWordInverseDocumentFreq(const std::string_view word) const; 
    
    template <typename DocumentPredicate,
              typename Policy>
    FindResult                                          FindAllDocuments(Policy& policy,
                                                                         const Query& query,
                                                                         DocumentPredicate document_predicate) const; 
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename DocumentPredicate>
SearchServer::FindResult SearchServer::FindTopDocuments(const std::string_view raw_query,
                                                        DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename Policy>
SearchServer::FindResult SearchServer::FindTopDocuments(Policy& policy,
                                                        const std::string_view raw_query,
                                                        DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    std::sort(policy,
        matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
        return (std::abs(lhs.relevance - rhs.relevance) < 1e-6 && lhs.rating > rhs.rating)
            || (lhs.relevance > rhs.relevance);
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Policy>
SearchServer::FindResult SearchServer::FindTopDocuments(Policy& policy,
                                                        const std::string_view raw_query,
                                                        DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int, DocumentStatus document_status, int) {
        return document_status == status;
    });
}

template <typename Policy>
SearchServer::FindResult SearchServer::FindTopDocuments(Policy& policy,
                                                        const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename Policy>
SearchServer::MatchDocumentResult SearchServer::MatchDocument(Policy& policy,
                                                              const std::string_view raw_query,
                                                              int document_id) const {
    const auto query = ParseQuery(raw_query);
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("This document id doesn't exist");
    }
    VectorStringView matched_words;
    std::for_each(policy,
                query.plus_words.begin(),
                query.plus_words.end(),
                [this, &matched_words, document_id] (const std::string_view word) {
                    if (word_to_document_freqs_.at(word).count(document_id) > 0) {
                        auto it = documents_.at(document_id).word_to_freqs.find(word);
                        matched_words.push_back(it->first);
                    }
                });
    std::for_each(policy,
                query.minus_words.begin(),
                query.minus_words.end(),
                [this, &matched_words, document_id] (const std::string_view word) {
                    if (word_to_document_freqs_.at(word).count(document_id) > 0) {
                        matched_words.clear();
                    }
                });
    return {matched_words, documents_.at(document_id).status};
}

template <typename Policy>
void SearchServer::RemoveDocument(Policy& policy,
                                  int document_id) { 
    if (documents_.count(document_id) == 0) {
        return;
    }
    std::for_each(policy,
            documents_.at(document_id).word_to_freqs.begin(),
            documents_.at(document_id).word_to_freqs.end(),
            [this, document_id] (const auto& word) {
                word_to_document_freqs_[word.first].erase(document_id); 
            });
    documents_.erase(document_id); 
    document_ids_.erase(document_id);
} 

template <typename DocumentPredicate, typename Policy>
SearchServer::FindResult SearchServer::FindAllDocuments(Policy& policy,
                                                        const SearchServer::Query& query,
                                                        DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance;
    std::for_each(policy,
        query.plus_words.begin(), query.plus_words.end(),
        [this, &document_to_relevance, &document_predicate, &policy] (const std::string_view word) {
            if (word_to_document_freqs_.count(word) > 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                std::for_each(policy,
                    word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(),
                    [this, &document_to_relevance, &document_predicate, &inverse_document_freq] (const auto& pair) {
                        const auto& document_data = documents_.at(pair.first);
                        if (document_predicate(pair.first, document_data.status, document_data.rating)) {
                            document_to_relevance[pair.first].ref_to_value += pair.second * inverse_document_freq;
                        }
                });
            }
        });
    std::for_each(policy,
        query.minus_words.begin(), query.minus_words.end(),
        [this, &document_to_relevance, &policy] (const std::string_view word) {
            if (word_to_document_freqs_.count(word) > 0) {
                std::for_each(policy,
                    word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(),
                    [&document_to_relevance] (const auto& pair) {
                        document_to_relevance.Erase(pair.first);
                });
            }
        });
    auto result = document_to_relevance.BuildOrdinaryMap();
    FindResult matched_documents(result.size());
    std::atomic_int index = 0;
    std::for_each(policy,
        result.begin(), result.end(),
        [this, &matched_documents, &index] (const auto& pair) {
            matched_documents[index++] = Document(pair.first, pair.second, documents_.at(pair.first).rating);
    });
    return matched_documents;
}