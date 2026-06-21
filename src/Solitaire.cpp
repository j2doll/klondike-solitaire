#include "Solitaire.h"
#include <algorithm>
#include <random>
#include <ctime>
#include <iostream>
#include <fstream>

#ifndef EMSCRIPTEN_BUILD
#include <nlohmann/json.hpp>

using json = nlohmann::json;
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern float gameScale;

Solitaire::Solitaire() {
    // Initialize random seed
    srand((unsigned int)time(NULL));

    // Initialize game state
    menuOpen = false;
    helpMenuOpen = false;
    shouldClose = false;
    aboutDialogOpen = false;
    gameWon = false;
    draggedSourcePile = nullptr;
    lastDrawnCard = nullptr;
    lastDealTime = 0.0;

    // Initialize piles
    tableau.resize(7);
    foundations.resize(4);

    // Load cards
    std::string currentDir = GetWorkingDirectory();
    std::string cardBackPath = "assets/cards/card_back_red.png";
    if (!FileExists(cardBackPath.c_str())) {
        cardBackPath = currentDir + "/assets/cards/card_back_red.png";
    }
    Card::loadCardBack(cardBackPath);
    loadCards();
    resetGame();
}

Solitaire::~Solitaire() {
    // Clean up all textures
    Card::unloadAllTextures();
}

void Solitaire::resetGame() {
    // Clear all piles but keep the textures
    for (auto& pile : tableau) {
        pile.clear();
    }
    for (auto& pile : foundations) {
        pile.clear();
    }
    stock.clear();
    waste.clear();
    draggedCards.clear();
    draggedSourcePile = nullptr;
    gameWon = false;

    // Initialize tableau and foundations
    tableau.resize(7);
    foundations.resize(4);

    // Create a new deck by reusing existing textures
    const std::string suits[] = {"hearts", "diamonds", "clubs", "spades"};
    const std::string values[] = {"ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "jack", "queen", "king"};

    for (const auto& suit : suits) {
        for (const auto& value : values) {
            std::string imagePath = "assets/cards/" + value + "_of_" + suit + ".png";
            stock.emplace_back(suit, value, imagePath);
        }
    }

    // Shuffle the deck
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(stock.begin(), stock.end(), g);

    dealCards();
}

void Solitaire::loadCards() {
    const std::string suits[] = {"hearts", "diamonds", "clubs", "spades"};
    const std::string values[] = {"ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "jack", "queen", "king"};

    // Get the current working directory
    std::string currentDir = GetWorkingDirectory();

    for (const auto& suit : suits) {
        for (const auto& value : values) {
            // Try both possible paths
            std::string imagePath = "assets/cards/" + value + "_of_" + suit + ".png";
            if (!FileExists(imagePath.c_str())) {
                // Try alternative path
                imagePath = currentDir + "/assets/cards/" + value + "_of_" + suit + ".png";
                if (!FileExists(imagePath.c_str())) {
                    std::cerr << "Could not find card image for: " << value << " of " << suit << std::endl;
                    continue;
                }
            }
            stock.emplace_back(suit, value, imagePath);
        }
    }

    // Shuffle the deck
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(stock.begin(), stock.end(), g);
}

void Solitaire::dealCards() {
    // Deal cards to tableau piles
    for (int i = 0; i < 7; i++) {
        for (int j = i; j < 7; j++) {
            if (!stock.empty()) {
                Card card = stock.back();
                stock.pop_back();
                // The first card to be added to each pile (when j == i) should be face up
                if (j == i) {
                    card.flip();
                }
                // Add to back of vector (top of pile)
                tableau[j].push_back(card);
            }
        }
    }
}

std::vector<Card>* Solitaire::getPileAtPos(Vector2 pos) {
    // Check tableau piles (moved down by MENU_HEIGHT)
    for (int i = 0; i < 7; i++) {
        float x = 50 + float(i) * float(baseTableauSpacing);
        float y = 130 + float(baseMenuHeight);  // Changed from 150 to 130
        
        // Check if click is within the pile's x-range
        if (x <= pos.x && pos.x <= x + float(baseCardWidth)) {
            // If pile has cards, check each card's position
            if (!tableau[i].empty()) {
                float cardY = y;
                for (size_t j = 0; j < tableau[i].size(); j++) {
                    if (tableau[i][j].isFaceUp()) {
                        tableau[i][j].setPosition(x, cardY);
                        if (CheckCollisionPointRec(pos, tableau[i][j].getRect())) {
                            return &tableau[i];
                        }
                    }
                    cardY += baseCardSpacing;
                }
            } else {
                // Empty tableau pile - check if click is in the empty space
                Rectangle emptyRect = { x, y, static_cast<float>(baseCardWidth), static_cast<float>(baseCardHeight) };
                if (CheckCollisionPointRec(pos, emptyRect)) {
                    return &tableau[i];
                }
            }
        }
    }

    // Check foundation piles (moved down by MENU_HEIGHT)
    for (int i = 0; i < 4; i++) {
        float x = 50 + float(i) * float(baseTableauSpacing);
        float y = 10 + float(baseMenuHeight);  // Add MENU_HEIGHT
        
        // Check if click is within the pile's x-range and y-range
        if (x <= pos.x && pos.x <= x + float(baseCardWidth) && y <= pos.y && pos.y <= y + float(baseCardHeight)) {
            if (!foundations[i].empty()) {
                foundations[i].back().setPosition(x, y);
                if (CheckCollisionPointRec(pos, foundations[i].back().getRect())) {
                    return &foundations[i];
                }
            } else {
                // Empty foundation pile
                Rectangle emptyRect = { x, y, static_cast<float>(baseCardWidth), static_cast<float>(baseCardHeight) };
                if (CheckCollisionPointRec(pos, emptyRect)) {
                    return &foundations[i];
                }
            }
        }
    }

    // Check stock pile
    float stockX = 50;
    float stockY = baseWindowHeight - baseCardHeight - 20;
    Rectangle stockRect = { stockX, stockY, static_cast<float>(baseCardWidth), static_cast<float>(baseCardHeight) };
    if (CheckCollisionPointRec(pos, stockRect)) {
        return &stock;
    }

    // Check waste pile
    float wasteX = stockX + baseTableauSpacing;
    float wasteY = baseWindowHeight - baseCardHeight - 20;
    if (!waste.empty()) {
        waste.back().setPosition(wasteX, wasteY);
        if (CheckCollisionPointRec(pos, waste.back().getRect())) {
            return &waste;
        }
    } else {
        // Empty waste pile
        Rectangle emptyRect = { wasteX, wasteY, static_cast<float>(baseCardWidth), static_cast<float>(baseCardHeight) };
        if (CheckCollisionPointRec(pos, emptyRect)) {
            return &waste;
        }
    }

    return nullptr;
}

bool Solitaire::canMoveToTableau(const Card& card, const std::vector<Card>& targetPile) {
    if (targetPile.empty()) {
        // Only kings can be placed on empty tableau
        return card.getValue() == 13;  // 13 represents king
    }
    
    const Card& topCard = targetPile.back();
    
    // Check if colors are different and values are in sequence
    bool colorsDifferent = (card.isRed() != topCard.isRed());
    bool valuesInSequence = (card.getValue() == topCard.getValue() - 1);
    
    return colorsDifferent && valuesInSequence;
}

std::string Solitaire::getNextValue(const std::string& value) {
    if (value == "king") return "queen";
    if (value == "queen") return "jack";
    if (value == "jack") return "10";
    if (value == "10") return "9";
    if (value == "9") return "8";
    if (value == "8") return "7";
    if (value == "7") return "6";
    if (value == "6") return "5";
    if (value == "5") return "4";
    if (value == "4") return "3";
    if (value == "3") return "2";
    if (value == "2") return "ace";
    return "";  // No next value for ace
}

bool Solitaire::canMoveToFoundation(const Card& card, const std::vector<Card>& targetPile) {
    if (targetPile.empty()) {
        // Only aces can start a foundation pile
        return card.getValue() == 1; 
    }
    
    const Card& topCard = targetPile.back();
    // Cards must be of the same suit and in ascending order (A,2,3,...)
    return (card.getSuit() == topCard.getSuit()) && (card.getValue() == topCard.getValue() + 1);
}

bool Solitaire::moveCards(std::vector<Card>& sourcePile, std::vector<Card>& targetPile, 
                         int startIndex, int endIndex) {
    if (startIndex < 0 || startIndex >= sourcePile.size()) {
        return false;
    }

    if (endIndex == -1) {
        endIndex = int(sourcePile.size()) - 1;
    }

    // Move cards
    for (int i = startIndex; i <= endIndex; i++) {
        targetPile.push_back(sourcePile[i]);
    }
    sourcePile.erase(sourcePile.begin() + startIndex, sourcePile.begin() + endIndex + 1);

    // Flip the new top card of the source pile if it exists
    if (!sourcePile.empty() && !sourcePile.back().isFaceUp()) {
        sourcePile.back().flip();
    }

    return true;
}

void Solitaire::handleMouseDown(Vector2 pos) {
    // Calculate the offset to center the game in the window (same as in main.cpp)
    float offsetX = (GetScreenWidth() - (baseWindowWidth * gameScale)) * 0.5f;
    float offsetY = (GetScreenHeight() - (baseWindowHeight * gameScale)) * 0.5f;
    
    // Transform mouse position to game coordinates (inverse of the rendering transform)
    pos.x = (pos.x - offsetX) / gameScale;
    pos.y = (pos.y - offsetY) / gameScale;

    // Check stock pile first
    float stockX = 50;
    float stockY = baseWindowHeight - baseCardHeight - 20;
    Rectangle stockRect = { stockX, stockY, static_cast<float>(baseCardWidth), static_cast<float>(baseCardHeight) };
    if (CheckCollisionPointRec(pos, stockRect)) {
        if (stock.empty() && !waste.empty()) {
            // Only restore waste cards if stock is empty and waste is not empty
            while (!waste.empty()) {
                Card card = waste.back();
                waste.pop_back();
                card.flip();  // Flip face down
                stock.push_back(card);
            }
            lastDrawnCard = nullptr;  // Reset last drawn card when recycling waste
            return;  // Return here to prevent any further handling
        }
        
        // Only deal a card if stock is not empty
        if (!stock.empty()) {
            Card card = stock.back();
            stock.pop_back();
            card.flip();  // Flip face up
            waste.push_back(card);
            lastDrawnCard = &waste.back();  // Track the last drawn card
            lastDealTime = GetTime();
        }

        return;  // Return after handling stock pile
    }

    // Find the card that was clicked (account for MENU_HEIGHT in foundation and tableau)
    for (int i = 0; i < 7; i++) {
        float x = 50 + float(i) * float(baseTableauSpacing);
        float y = 130 + float(baseMenuHeight);
        
        // Check if click is within the pile's x-range
        if (x <= pos.x && pos.x <= x + float(baseCardWidth)) {
            // Find the actual card that was clicked by checking the y-position
            float baseY = y;
            float clickedY = pos.y;
            
            // Calculate which card was actually clicked based on y-position
            int clickedIndex = static_cast<int>((clickedY - baseY) / baseCardSpacing);
            if (clickedIndex < 0) clickedIndex = 0;
            if (clickedIndex >= static_cast<int>(tableau[i].size())) clickedIndex = static_cast<int>(tableau[i].size()) - 1;
            
            // Only select if the card at this position is face up and we're actually clicking on its rectangle
            if (tableau[i][clickedIndex].isFaceUp()) {
                // Set the card's position to check for collision
                tableau[i][clickedIndex].setPosition(x, baseY + clickedIndex * baseCardSpacing);
                if (CheckCollisionPointRec(pos, tableau[i][clickedIndex].getRect())) {
                    draggedCards.clear();
                    for (int j = clickedIndex; j < tableau[i].size(); j++) {
                        draggedCards.push_back(tableau[i][j]);
                    }
                    draggedStartIndex = clickedIndex;
                    draggedSourcePile = &tableau[i];
                    
                    // Calculate offset from mouse position to card position
                    dragOffset = {
                        pos.x - x,
                        pos.y - (baseY + clickedIndex * baseCardSpacing)
                    };
                    break;
                }
            }
        }
        if (!draggedCards.empty()) break;
    }

    // Check foundation piles if no card was found in tableau
    if (draggedCards.empty()) {
        for (int i = 0; i < 4; i++) {
            float x = 50 + float(i) * float(baseTableauSpacing);
            float y = 10 + float(baseMenuHeight);
            
            if (!foundations[i].empty()) {
                foundations[i].back().setPosition(x, y);
                if (CheckCollisionPointRec(pos, foundations[i].back().getRect())) {
                    draggedCards.clear();
                    draggedCards.push_back(foundations[i].back());
                    draggedStartIndex = int(foundations[i].size()) - 1;
                    draggedSourcePile = &foundations[i];
                    
                    // Calculate offset from mouse position to card position
                    dragOffset = {
                        pos.x - x,
                        pos.y - y
                    };
                    break;
                }
            }
        }
    }

    // Check waste pile if no card was found in tableau or foundation
    if (draggedCards.empty() && !waste.empty()) {
        float wasteX = stockX + baseTableauSpacing;
        float wasteY = baseWindowHeight - baseCardHeight - 20;
        waste.back().setPosition(wasteX, wasteY);
        if (CheckCollisionPointRec(pos, waste.back().getRect())) {
            draggedCards.clear();
            draggedCards.push_back(waste.back());
            draggedStartIndex = 0;
            draggedSourcePile = &waste;
            
            // Calculate offset from mouse position to card position
            dragOffset = {
                pos.x - wasteX,
                pos.y - wasteY
            };
        }
    }
}

void Solitaire::handleMouseUp(Vector2 pos) {
    // Calculate the offset to center the game in the window (same as in main.cpp)
    float offsetX = (GetScreenWidth() - (baseWindowWidth * gameScale)) * 0.5f;
    float offsetY = (GetScreenHeight() - (baseWindowHeight * gameScale)) * 0.5f;
    
    // Transform mouse position to game coordinates (inverse of the rendering transform)
    pos.x = (pos.x - offsetX) / gameScale;
    pos.y = (pos.y - offsetY) / gameScale;

    // If we're not dragging any cards, there's nothing to do
    if (draggedCards.empty()) return;

    // Make sure we have a valid source pile
    if (!draggedSourcePile) {
        draggedCards.clear();
        return;
    }

    std::vector<Card>* targetPile = getPileAtPos(pos);
    if (!targetPile) {
        // Return cards to original position
        returnDraggedCards();
        draggedCards.clear();
        draggedSourcePile = nullptr;
        return;
    }

    if (targetPile == draggedSourcePile) {
        // Dropping cards back onto their original pile, just clear the dragged state
        returnDraggedCards();
        draggedCards.clear();
        draggedSourcePile = nullptr;
        return;
    }

    // Check if target pile is the waste pile - never allow dropping cards on the waste pile
    if (targetPile == &waste) {
        returnDraggedCards();
        draggedCards.clear();
        draggedSourcePile = nullptr;
        return;
    }

    // Check if target pile is the stock pile - never allow dropping cards on the stock pile
    if (targetPile == &stock) {
        returnDraggedCards();
        draggedCards.clear();
        draggedSourcePile = nullptr;
        return;
    }

    // Check if target pile is a foundation pile
    bool isFoundationPile = false;
    for (auto& foundation : foundations) {
        if (targetPile == &foundation) {
            isFoundationPile = true;
            break;
        }
    }

    // Handle foundation pile placement - only single cards, must follow suit and sequence
    if (isFoundationPile) {
        // Only single cards can be moved to foundation
        if (draggedCards.size() == 1 && canMoveToFoundation(draggedCards[0], *targetPile)) {
            moveCards(*draggedSourcePile, *targetPile, int(draggedSourcePile->size() - 1));
        } else {
            // Invalid move to foundation
            returnDraggedCards();
        }
    }
    // Handle tableau pile placement
    else {
        // Check if we can move to tableau
        bool canMove = canMoveToTableau(draggedCards[0], *targetPile);

        if (canMove) {
            // If dragging from waste pile or foundation pile, only move the top card
            if (draggedSourcePile == &waste || 
                draggedSourcePile == &foundations[0] || draggedSourcePile == &foundations[1] || 
                draggedSourcePile == &foundations[2] || draggedSourcePile == &foundations[3]) {
                moveCards(*draggedSourcePile, *targetPile, int(draggedSourcePile->size() - 1));
            } else {
                moveCards(*draggedSourcePile, *targetPile, draggedStartIndex);
            }
        } else {
            // Invalid move to tableau
            returnDraggedCards();
        }
    }

    // Clean up the dragged state
    draggedCards.clear();
    draggedSourcePile = nullptr;
}

// Add this helper method to avoid code duplication
void Solitaire::returnDraggedCards() {
    if (!draggedSourcePile || draggedCards.empty()) {
        return; // Safety check
    }

    if (draggedSourcePile == &waste) {
        float stockX = 50;
        float wasteX = stockX + baseTableauSpacing;
        float wasteY = baseWindowHeight - baseCardHeight - 20;
        for (size_t i = 0; i < draggedCards.size(); i++) {
            draggedCards[i].setPosition(wasteX, wasteY);
        }
    } else if (draggedSourcePile == &foundations[0] || draggedSourcePile == &foundations[1] || 
              draggedSourcePile == &foundations[2] || draggedSourcePile == &foundations[3]) {
        // Find the original foundation pile
        for (int i = 0; i < 4; i++) {
            if (&foundations[i] == draggedSourcePile) {
                float x = 50 + float(i) * float(baseTableauSpacing);
                float y = 10 + float(baseMenuHeight);
                for (size_t j = 0; j < draggedCards.size(); j++) {
                    draggedCards[j].setPosition(x, y);
                }
                break;
            }
        }
    } else {
        // Find the original tableau pile
        for (int i = 0; i < 7; i++) {
            if (&tableau[i] == draggedSourcePile) {
                float x = 50 + float(i) * float(baseTableauSpacing);
                float y = 130 + float(baseMenuHeight);
                for (size_t j = 0; j < draggedCards.size(); j++) {
                    draggedCards[j].setPosition(x, y + (draggedStartIndex + j) * float(baseCardSpacing));
                }
                break;
            }
        }
    }
}

void Solitaire::handleDoubleClick(Vector2 pos) {
    // Calculate the offset to center the game in the window (same as in main.cpp)
    float offsetX = (GetScreenWidth() - (baseWindowWidth * gameScale)) * 0.5f;
    float offsetY = (GetScreenHeight() - (baseWindowHeight * gameScale)) * 0.5f;
    
    // Transform mouse position to game coordinates (inverse of the rendering transform)
    pos.x = (pos.x - offsetX) / gameScale;
    pos.y = (pos.y - offsetY) / gameScale;

    std::vector<Card>* pile = getPileAtPos(pos);
    if (!pile || pile->empty()) return;

    // Don't allow double-clicking on waste pile if the card was just dealt
    if (pile == &waste && GetTime() - lastDealTime < 0.5) {  // 500ms delay
        return;
    }

    Card& card = pile->back();
    // Only allow double-clicking on face-up cards
    if (!card.isFaceUp()) return;

    std::vector<Card>* foundationPile = findValidFoundationPile(card);
    if (foundationPile) {
        moveCards(*pile, *foundationPile, int(pile->size() - 1));
    }
}

std::vector<Card>* Solitaire::findValidFoundationPile(const Card& card) {
    for (auto& foundation : foundations) {
        if (canMoveToFoundation(card, foundation)) {
            return &foundation;
        }
    }
    return nullptr;
}

bool Solitaire::checkWin() {
    for (const auto& foundation : foundations) {
        if (foundation.empty() || foundation.back().getValue() != 13) {
            return false;
        }
    }
    return true;
}

bool Solitaire::saveGame() {
#ifndef EMSCRIPTEN_BUILD
    std::ofstream file("solitaire_save.txt");
    if (!file.is_open()) {
        return false;
    }
    
    try {
        json gameState;
        
        // Save tableau piles
        for (const auto& pile : tableau) {
            json pileJson;
            for (const auto& card : pile) {
                json cardJson;
                cardJson["suit"] = card.getSuit();
                cardJson["value"] = card.getValue();
                cardJson["faceUp"] = card.isFaceUp();
                pileJson.push_back(cardJson);
            }
            gameState["tableau"].push_back(pileJson);
        }
        
        // Save foundation piles
        for (const auto& pile : foundations) {
            json pileJson;
            for (const auto& card : pile) {
                json cardJson;
                cardJson["suit"] = card.getSuit();
                cardJson["value"] = card.getValue();
                cardJson["faceUp"] = card.isFaceUp();
                pileJson.push_back(cardJson);
            }
            gameState["foundations"].push_back(pileJson);
        }
        
        // Save stock pile
        for (const auto& card : stock) {
            json cardJson;
            cardJson["suit"] = card.getSuit();
            cardJson["value"] = card.getValue();
            cardJson["faceUp"] = card.isFaceUp();
            gameState["stock"].push_back(cardJson);
        }
        
        // Save waste pile
        for (const auto& card : waste) {
            json cardJson;
            cardJson["suit"] = card.getSuit();
            cardJson["value"] = card.getValue();
            cardJson["faceUp"] = card.isFaceUp();
            gameState["waste"].push_back(cardJson);
        }
        
        file << gameState.dump(4);
        file.close();
        return true;
    } catch (const std::exception& ) { // e) {
        return false;
    }
#endif
    return false;  // Return false if not compiled with PLATFORM_DESKTOP
}

bool Solitaire::loadGame() {
#ifndef EMSCRIPTEN_BUILD
    std::ifstream file("solitaire_save.txt");
    if (!file.is_open()) {
        return false;
    }
    
    try {
        json gameState;
        file >> gameState;
        file.close();
        
        // Clear current game state
        tableau.clear();
        foundations.clear();
        stock.clear();
        waste.clear();
        draggedCards.clear();
        draggedSourcePile = nullptr;
        gameWon = false;
        
        // Initialize tableau and foundations
        tableau.resize(7);
        foundations.resize(4);
        
        // Load tableau piles
        for (size_t i = 0; i < gameState["tableau"].size(); i++) {
            for (const auto& cardData : gameState["tableau"][i]) {
                std::string suit = cardData["suit"];
                int value = cardData["value"];
                bool faceUp = cardData["faceUp"];
                
                // Convert value back to string
                std::string valueStr;
                if (value == 1) valueStr = "ace";
                else if (value == 11) valueStr = "jack";
                else if (value == 12) valueStr = "queen";
                else if (value == 13) valueStr = "king";
                else valueStr = std::to_string(value);
                
                // Create card
                std::string imagePath = "assets/cards/" + valueStr + "_of_" + suit + ".png";
                Card card(suit, valueStr, imagePath);
                if (faceUp) card.flip();
                tableau[i].push_back(card);
            }
        }
        
        // Load foundation piles
        for (size_t i = 0; i < gameState["foundations"].size(); i++) {
            for (const auto& cardData : gameState["foundations"][i]) {
                std::string suit = cardData["suit"];
                int value = cardData["value"];
                bool faceUp = cardData["faceUp"];
                
                // Convert value back to string
                std::string valueStr;
                if (value == 1) valueStr = "ace";
                else if (value == 11) valueStr = "jack";
                else if (value == 12) valueStr = "queen";
                else if (value == 13) valueStr = "king";
                else valueStr = std::to_string(value);
                
                // Create card
                std::string imagePath = "assets/cards/" + valueStr + "_of_" + suit + ".png";
                Card card(suit, valueStr, imagePath);
                if (faceUp) card.flip();
                foundations[i].push_back(card);
            }
        }
        
        // Load stock pile
        for (const auto& cardData : gameState["stock"]) {
            std::string suit = cardData["suit"];
            int value = cardData["value"];
            bool faceUp = cardData["faceUp"];
            
            // Convert value back to string
            std::string valueStr;
            if (value == 1) valueStr = "ace";
            else if (value == 11) valueStr = "jack";
            else if (value == 12) valueStr = "queen";
            else if (value == 13) valueStr = "king";
            else valueStr = std::to_string(value);
            
            // Create card
            std::string imagePath = "assets/cards/" + valueStr + "_of_" + suit + ".png";
            Card card(suit, valueStr, imagePath);
            if (faceUp) card.flip();
            stock.push_back(card);
        }
        
        // Load waste pile
        for (const auto& cardData : gameState["waste"]) {
            std::string suit = cardData["suit"];
            int value = cardData["value"];
            bool faceUp = cardData["faceUp"];
            
            // Convert value back to string
            std::string valueStr;
            if (value == 1) valueStr = "ace";
            else if (value == 11) valueStr = "jack";
            else if (value == 12) valueStr = "queen";
            else if (value == 13) valueStr = "king";
            else valueStr = std::to_string(value);
            
            // Create card
            std::string imagePath = "assets/cards/" + valueStr + "_of_" + suit + ".png";
            Card card(suit, valueStr, imagePath);
            if (faceUp) card.flip();
            waste.push_back(card);
        }
        
        return true;
    } catch (const std::exception& ) { // e) {
        return false;
    }
#endif
    return false;  // Return false if not compiled with PLATFORM_DESKTOP
}

void Solitaire::handleMenuClick(Vector2 pos) {
    // Calculate the offset to center the game in the window (same as in main.cpp)
    float offsetX = (GetScreenWidth() - (baseWindowWidth * gameScale)) * 0.5f;
    float offsetY = (GetScreenHeight() - (baseWindowHeight * gameScale)) * 0.5f;
    
    // Transform mouse position to game coordinates (inverse of the rendering transform)
    pos.x = (pos.x - offsetX) / gameScale;
    pos.y = (pos.y - offsetY) / gameScale;

    // Check if clicking on menu bar
    if (pos.y < baseMenuItemHeight) {
        // Check if clicking on File menu
        if (pos.x >= baseMenuFileX && pos.x < baseMenuFileX + baseMenuFileWidth) {
            menuOpen = !menuOpen;
            helpMenuOpen = false;  // Close Help menu when opening File menu
        }
        // Check if clicking on Help menu
        else if (pos.x >= baseMenuHelpX && pos.x < baseMenuHelpX + baseMenuHelpWidth) {
            helpMenuOpen = !helpMenuOpen;
            menuOpen = false;  // Close File menu when opening Help menu
        }
        else {
            menuOpen = false;
            helpMenuOpen = false;
        }
    }
    // Check if clicking on File menu items
    else if (menuOpen && pos.y >= baseMenuItemHeight && pos.y < baseMenuItemHeight + baseMenuDropdownHeight) {
        if (pos.x >= baseMenuFileX && pos.x < baseMenuFileX + baseMenuFileWidth) {
            int itemIndex = static_cast<int>((pos.y - baseMenuItemHeight) / baseMenuItemHeight);
            switch (itemIndex) {
                case 0: // New Game
                    resetGame();
                    break;
                case 1: // Save
                    saveGame();
                    break;
                case 2: // Load
                    loadGame();
                    break;
                case 3: // Exit
                    shouldClose = true;
                    break;
            }
            menuOpen = false;
        }
    }
    // Check if clicking on Help menu items
    else if (helpMenuOpen && pos.y >= baseMenuItemHeight && pos.y < baseMenuItemHeight + baseMenuHelpDropdownHeight) {
        if (pos.x >= baseMenuHelpX && pos.x < baseMenuHelpX + baseMenuHelpWidth) {
            int itemIndex = static_cast<int>((pos.y - baseMenuItemHeight) / baseMenuItemHeight);
            if (itemIndex == 0) { // About
                showAboutDialog();
            }
            helpMenuOpen = false;
        }
    }
    else {
        menuOpen = false;
        helpMenuOpen = false;
    }
}

void Solitaire::showAboutDialog() {
    aboutDialogOpen = true;
}

void Solitaire::handleRightClick(Vector2 pos) {
    // Calculate the offset to center the game in the window (same as in main.cpp)
    float offsetX = (GetScreenWidth() - (baseWindowWidth * gameScale)) * 0.5f;
    float offsetY = (GetScreenHeight() - (baseWindowHeight * gameScale)) * 0.5f;
    
    // Transform mouse position to game coordinates (inverse of the rendering transform)
    pos.x = (pos.x - offsetX) / gameScale;
    pos.y = (pos.y - offsetY) / gameScale;

    // Check if clicking on stock pile area
    float stockX = 50;
    float stockY = baseWindowHeight - baseCardHeight - 20;
    Rectangle stockRect = { stockX, stockY, static_cast<float>(baseCardWidth), static_cast<float>(baseCardHeight) };
    
    if (CheckCollisionPointRec(pos, stockRect) && lastDrawnCard != nullptr && !waste.empty() && lastDrawnCard == &waste.back()) {
        // Move the last drawn card back to stock
        Card card = waste.back();
        waste.pop_back();
        card.flip();  // Flip face down
        stock.push_back(card);
        lastDrawnCard = nullptr;  // Reset last drawn card after undo
    }
}

void Solitaire::update() {
    static int frameCount = 0;
    frameCount++;

    if (frameCount % 60 == 0) {  // Log every second (assuming 60 FPS)
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 pos = GetMousePosition();
        
        // Check menu first
        handleMenuClick(pos);
        
        // Then check game interactions
        if (!menuOpen) {
            handleMouseDown(pos);
        }
    }
    
    // Always check for mouse button release to ensure we stop dragging cards
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        Vector2 pos = GetMousePosition();
        handleMouseUp(pos);
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && GetMouseDelta().x == 0 && GetMouseDelta().y == 0) {
        // Check for double click (mouse hasn't moved between clicks)
        static double lastClickTime = 0;
        double currentTime = GetTime();
        if (currentTime - lastClickTime < 0.3) {  // 300ms threshold for double click
            handleDoubleClick(GetMousePosition());
        }
        lastClickTime = currentTime;
    }
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        Vector2 pos = GetMousePosition();
        handleRightClick(pos);
    }

    if (checkWin()) {
        gameWon = true;
    }
}

void Solitaire::draw() {
    static int drawCount = 0;
    drawCount++;

    ClearBackground(GREEN);

    // Draw foundation piles (moved down by MENU_HEIGHT)
    for (int i = 0; i < 4; i++) {
        float x = 50 + float(i) * float(baseTableauSpacing);
        float y = 10 + float(baseMenuHeight);  // Add MENU_HEIGHT
        if (!foundations[i].empty()) {
            // If this foundation pile is the source of the dragged card, show the card underneath
            if (draggedSourcePile == &foundations[i] && foundations[i].size() > 1) {
                foundations[i][foundations[i].size() - 2].setPosition(x, y);
                foundations[i][foundations[i].size() - 2].draw();
            } else if (draggedSourcePile != &foundations[i]) {
                // Otherwise show the top card if it's not being dragged
                foundations[i].back().setPosition(x, y);
                foundations[i].back().draw();
            }
        } else {
            // Draw empty foundation slot
            DrawRectangle(int(x), int(y), int(baseCardWidth), int(baseCardHeight), WHITE);
            DrawRectangleLines(int(x), int(y), int(baseCardWidth), int(baseCardHeight), BLACK);
        }
    }

    // Draw tableau piles (moved down by MENU_HEIGHT)
    for (int i = 0; i < 7; i++) {
        float x = 50 + float(i) * float(baseTableauSpacing);
        float y = 130 + float(baseMenuHeight);
         
        for (size_t j = 0; j < tableau[i].size(); j++) {
            // Skip drawing cards that are being dragged
            if (draggedSourcePile == &tableau[i] && j >= draggedStartIndex) {
                continue;
            }
            tableau[i][j].setPosition(x, y + j * baseCardSpacing);
            tableau[i][j].draw();
        }
    }

    // Draw stock pile
    float stockX = 50;
    float stockY = baseWindowHeight - baseCardHeight - 20;
    if (!stock.empty()) {
        // Skip drawing the stock card if it's being dragged
        if (draggedSourcePile != &stock) {
            // Draw a stack of cards for the stock pile
            int numCards = int(stock.size());
            int maxVisibleCards = 5;  // Maximum number of cards to show in the stack
            int cardsToShow = std::min(numCards, maxVisibleCards);
            
            for (int i = 0; i < cardsToShow; i++) {
                // Calculate offset for each card in the stack
                float offsetX = float(i) * 2;  // Small horizontal offset
                float offsetY = float(i) * 2;  // Small vertical offset
                
                // Get the card from the end of the stock pile
                Card& card = stock[stock.size() - 1 - i];
                card.setPosition(stockX + offsetX, stockY + offsetY);
                card.draw();
            }
            
            // Always show the total number of cards
            int fontSize = static_cast<int>(20);
            DrawText(TextFormat("%d", numCards), 
                    int(stockX + float(baseCardWidth) - float(55)), 
                    int(stockY + float(baseCardHeight) - float(20)), 
                    fontSize, 
                    BLACK);
        }
    }

    // Draw waste pile
    float wasteX = stockX + float(baseTableauSpacing);
    float wasteY = float(baseWindowHeight) - float(baseCardHeight) - float(20);
    if (!waste.empty()) {
        // Skip drawing the waste card if it's being dragged
        if (draggedSourcePile != &waste) {
            waste.back().setPosition(wasteX, wasteY);
            waste.back().draw();
        }
    }

    // Draw dragged cards
    if (!draggedCards.empty()) {
        Vector2 mousePos = GetMousePosition();
        // Calculate the offset to center the game in the window (same as in main.cpp)
        float offsetX = (GetScreenWidth() - (baseWindowWidth * gameScale)) * 0.5f;
        float offsetY = (GetScreenHeight() - (baseWindowHeight * gameScale)) * 0.5f;
        
        // Transform mouse position to game coordinates (inverse of the rendering transform)
        mousePos.x = (mousePos.x - offsetX) / gameScale;
        mousePos.y = (mousePos.y - offsetY) / gameScale;

        for (size_t i = 0; i < draggedCards.size(); i++) {
            // Apply the drag offset to maintain the relative position
            draggedCards[i].setPosition(
                mousePos.x - dragOffset.x,
                mousePos.y - dragOffset.y + i * baseCardSpacing
            );
            draggedCards[i].draw();
        }
    }

    // Draw all UI elements last
    // Draw menu bar
    DrawRectangle(0, 0, baseWindowWidth, baseMenuHeight, DARKGRAY);
    int fontSize = static_cast<int>(20);
    DrawText("File", baseMenuFileX + baseMenuTextPadding, baseMenuTextPadding, fontSize, WHITE);
    DrawText("Help", baseMenuHelpX + baseMenuTextPadding, baseMenuTextPadding, fontSize, WHITE);
    
    // Draw menu items when File is clicked
    if (menuOpen) {
        DrawRectangle(baseMenuFileX, baseMenuHeight, baseMenuFileWidth, baseMenuDropdownHeight, DARKGRAY);
        DrawText("New Game", baseMenuFileX + baseMenuTextPadding, baseMenuHeight + baseMenuTextPadding, fontSize, WHITE);
#ifndef EMSCRIPTEN_BUILD        
        DrawText("Save", baseMenuFileX + baseMenuTextPadding, baseMenuHeight + baseMenuItemHeight + baseMenuTextPadding, fontSize, WHITE);
        DrawText("Load", baseMenuFileX + baseMenuTextPadding, baseMenuHeight + baseMenuItemHeight * 2 + baseMenuTextPadding, fontSize, WHITE);
        DrawText("Exit", baseMenuFileX + baseMenuTextPadding, baseMenuHeight + baseMenuItemHeight * 3 + baseMenuTextPadding, fontSize, WHITE);
#endif
    }

    // Draw Help menu dropdown if open
    if (helpMenuOpen) {
        DrawRectangle(baseMenuHelpX, baseMenuHeight, baseMenuHelpWidth, baseMenuHelpDropdownHeight, DARKGRAY);
        DrawRectangleLines(baseMenuHelpX, baseMenuHeight, baseMenuHelpWidth, baseMenuHelpDropdownHeight, WHITE);
        
        DrawText("About", 
                baseMenuHelpX + baseMenuTextPadding, 
                baseMenuHeight + baseMenuTextPadding, 
                20, WHITE);
    }

    // Draw About dialog if open
    if (aboutDialogOpen) {
        // Create a modal dialog with game information
        const char* aboutText = "Solitaire\n\n"
                               "Classic Klondike Solitaire\n\n"
                               "Controls:\n"
                               "- Drag cards to move them\n"
                               "- Double click to auto-move cards to foundation\n"
                               "- Use the menu for game options\n\n";

        // Calculate dialog dimensions
        int fontSize = 20;
        int textWidth = MeasureText(aboutText, fontSize);
        int dialogWidth = textWidth + 40;
        int dialogHeight = 300;
        int dialogX = (baseWindowWidth - dialogWidth) / 2;
        int dialogY = (baseWindowHeight - dialogHeight) / 2;

        // Draw dialog background
        DrawRectangle(dialogX, dialogY, dialogWidth, dialogHeight, LIGHTGRAY);
        DrawRectangleLines(dialogX, dialogY, dialogWidth, dialogHeight, DARKGRAY);

        // Draw text
        DrawText(aboutText, dialogX + 20, dialogY + 20, fontSize, BLACK);

        // Draw OK button
        const char* okText = "OK";
        int buttonWidth = 60;
        int buttonHeight = 30;
        int buttonX = dialogX + (dialogWidth - buttonWidth) / 2;
        int buttonY = dialogY + dialogHeight - buttonHeight - 20;
        
        // Draw button background
        DrawRectangle(buttonX, buttonY, buttonWidth, buttonHeight, DARKGRAY);
        DrawRectangleLines(buttonX, buttonY, buttonWidth, buttonHeight, BLACK);
        
        // Draw button text
        int textX = buttonX + (buttonWidth - MeasureText(okText, fontSize)) / 2;
        int textY = buttonY + (buttonHeight - fontSize) / 2;
        DrawText(okText, textX, textY, fontSize, WHITE);

        // Check if OK button is clicked
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();
            // Calculate the offset to center the game in the window (same as in main.cpp)
            float offsetX = (GetScreenWidth() - (baseWindowWidth * gameScale)) * 0.5f;
            float offsetY = (GetScreenHeight() - (baseWindowHeight * gameScale)) * 0.5f;
            
            // Transform mouse position to game coordinates (inverse of the rendering transform)
            mousePos.x = (mousePos.x - offsetX) / gameScale;
            mousePos.y = (mousePos.y - offsetY) / gameScale;

            if (mousePos.x >= buttonX && mousePos.x <= buttonX + buttonWidth &&
                mousePos.y >= buttonY && mousePos.y <= buttonY + buttonHeight) {
                aboutDialogOpen = false;
                helpMenuOpen = false;
            }
        }
    }

    if (gameWon) {
        fontSize = static_cast<int>(40);
        DrawText("You Win!", baseWindowWidth/2 - 100, baseWindowHeight/2, fontSize, WHITE);
    }
}