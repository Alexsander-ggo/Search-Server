#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(string_view(stop_words_text)) {}

SearchServer::SearchServer(const string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id,
                               const string_view document,
                               DocumentStatus status,
                               const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto [it, inserted] = documents_.emplace(document_id,
                                                   DocumentData{ComputeAverageRating(ratings),
                                                                status, string(document), {}}); 
    const auto words = SplitIntoWordsNoStop(it->second.data);
    const double inv_word_count = 1.0 / words.size();
    map<string_view, double> word_to_freqs;
    for (const string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_to_freqs[word] += inv_word_count;
    }
    documents_[document_id].word_to_freqs = word_to_freqs; 
    document_ids_.insert(document_id);
}

SearchServer::FindResult SearchServer::FindTopDocuments(const string_view raw_query,
                                                        DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, status);
}

SearchServer::FindResult SearchServer::FindTopDocuments(const string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const string_view raw_query,
                                                              int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

SearchServer::ConstIterator SearchServer::begin() const {
    return document_ids_.cbegin();
} 
    
SearchServer::ConstIterator SearchServer::end() const {
    return document_ids_.cend();
} 

const SearchServer::MapWordFreqs& SearchServer::GetWordFrequencies(int document_id) const { 
    static const MapWordFreqs EMPTY_MAP;
    if (documents_.count(document_id) > 0) { 
        return documents_.at(document_id).word_to_freqs; 
    } 
    return EMPTY_MAP; 
} 

void SearchServer::RemoveDocument(int document_id) { 
    RemoveDocument(execution::seq, document_id);
} 

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

VectorStringView SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    VectorStringView words;
    for (const string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view word) const {
    if (word.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word.remove_prefix(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + string(word) + " is invalid");
    }
    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const string_view text) const {
    SearchServer::Query result;
    for (const string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}