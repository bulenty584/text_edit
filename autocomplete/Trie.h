// Basic Trie Data Structure for Autocomplete

#ifndef TRIE_H
#define TRIE_H

#define ALPHABET_SIZE 26

typedef struct TrieNode {
    struct TrieNode* children[26];
    bool isWord;
} TrieNode;

TrieNode* trieCreateNode(void);

void trieInsertWord(TrieNode* root, const char* word);

bool trieSearch(TrieNode* root, const char* word);

void trieAutoComplete(TrieNode* root, const char* prefix);

int trieGetSuggestions(TrieNode* root, const char* prefix, char suggestions[][256], int max_count);

void trieFree(TrieNode* root);

#endif