#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "Trie.h"

void trieLoadDictionary(TrieNode* root, const char* filename){
    FILE *fp = fopen(filename, "r");

    if (!fp){
        printf("Couldn't open file %s\n", filename);
        exit(1);
    }

    char word[256];

    while (fgets(word, sizeof(word), fp)){
        word[strcspn(word, "\r\n")] = '\0';

        if (strlen(word) == 0) continue;

        // Convert to lowercase
        for (int i = 0; word[i]; i++) {
            if (word[i] >= 'A' && word[i] <= 'Z')
                word[i] = word[i] - 'A' + 'a';
        }

        // Skip empty lines
        trieInsertWord(root, word);
    }

    fclose(fp);
}

TrieNode *trieCreateNode(void){
    TrieNode* node = malloc(sizeof(TrieNode));

    if (!node) return NULL;

    for (int i = 0; i < ALPHABET_SIZE; i++){
        node->children[i] = NULL;
    }

    node->isWord = false;
    return node;
}

void trieInsertWord(TrieNode* root, const char* word){
    TrieNode* curr = root;

    while (*word){
        int index = *word - 'a';

        if (index < 0 || index >= ALPHABET_SIZE) {word++; continue;}

        if (!curr->children[index]) curr->children[index] = trieCreateNode();

        curr = curr->children[index];
        word++;
    }

    curr->isWord = true;

}

bool trieSearch(TrieNode* root, const char* word){
    TrieNode* curr = root;

    while (*word){
        int index = *word - 'a';

        if (index < 0 || index >= ALPHABET_SIZE){return false;}

        if (!curr->children[index]) return false;

        curr = curr->children[index];
        word++;
    }

    return curr && curr->isWord;
}

static void dfsSuggestions(TrieNode* node, char* buffer, int depth, int* count, int limit, char suggestions[][256]){
    if (!node || *count >= limit) return;
    if (node->isWord){
        buffer[depth] = '\0';
        strcpy(suggestions[*count], buffer);
        (*count)++;
    }

    for (int i = 0; i < ALPHABET_SIZE; i++){
        if (node->children[i]){
            buffer[depth] = 'a' + i;
            dfsSuggestions(node->children[i], buffer, depth + 1, count, limit, suggestions);
        }
    }
}

int trieGetSuggestions(TrieNode* root, const char* prefix, char suggestions[][256], int max_count){
    TrieNode* curr = root;
    const char* p = prefix;

    while (*p){
        int index = *p - 'a';
        if (index < 0 || index >= ALPHABET_SIZE || !curr->children[index]){
            return 0;
        }

        curr = curr->children[index];
        p++;
    }

    char buffer[256];
    strcpy(buffer, prefix);
    int count = 0;
    dfsSuggestions(curr, buffer, strlen(buffer), &count, max_count, suggestions);
    return count;
}

void trieFree(TrieNode* root){
    for (int i = 0; i < ALPHABET_SIZE; i++){
        if (root->children[i]) trieFree(root->children[i]);
    }

    free(root);
}

