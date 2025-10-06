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
        word[strcspn(word, "\r\n")] = 0;

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

static void dfsSuggestions(TrieNode* node, char* buffer, int depth){
    if (!node) return;
    if (node->isWord){
        buffer[depth] = '\0';
        printf("%s\n", buffer);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++){
        if (node->children[i]){
            buffer[depth] = 'a' + i;
            dfsSuggestions(node->children[i], buffer, depth + 1);
        }
    }
}

void trieAutoComplete(TrieNode* root, const char* prefix){
    TrieNode* curr = root;
    const char* p = prefix;

    while (*p){
        int index = *p - 'a';

        if (!curr->children[index]) return;

        curr = curr->children[index];
        p++;
    }

    char buffer[256];
    strcpy(buffer, prefix);
    dfsSuggestions(curr, buffer, strlen(buffer));

}

void trieFree(TrieNode* root){
    for (int i = 0; i < ALPHABET_SIZE; i++){
        if (root->children[i]) trieFree(root->children[i]);
    }

    free(root);
}

TrieNode *dictionary;

int main() {
    dictionary = trieCreateNode();
    trieLoadDictionary(dictionary, "words.txt");
    trieInsertWord(dictionary, "hello");
    trieInsertWord(dictionary, "help");
    trieInsertWord(dictionary, "helium");

    printf("Autocomplete for 'he':\n");
    trieAutoComplete(dictionary, "heroin");

    trieFree(dictionary);
    return 0;
}

