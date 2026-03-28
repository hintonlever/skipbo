#include <emscripten/bind.h>
#include "wasm_api.h"

using namespace emscripten;
using namespace skipbo;

EMSCRIPTEN_BINDINGS(skipbo) {
    register_vector<int>("VectorInt");
    register_vector<float>("VectorFloat");

    function("runMatch", &skipbo::wasm_run_match);
    function("runMatchLogged", &skipbo::wasm_run_match_logged);

    class_<WasmGameController>("GameController")
        .constructor<int>()
        // State queries
        .function("isGameOver", &WasmGameController::isGameOver)
        .function("getWinner", &WasmGameController::getWinner)
        .function("getCurrentPlayer", &WasmGameController::getCurrentPlayer)
        .function("getHand", &WasmGameController::getHand)
        .function("getStockTop", &WasmGameController::getStockTop)
        .function("getStockSize", &WasmGameController::getStockSize)
        .function("getBuildingPileTop", &WasmGameController::getBuildingPileTop)
        .function("getBuildingPileSize", &WasmGameController::getBuildingPileSize)
        .function("getDiscardTop", &WasmGameController::getDiscardTop)
        .function("getDiscardSize", &WasmGameController::getDiscardSize)
        .function("getDiscardPile", &WasmGameController::getDiscardPile)
        .function("getSkipBoPlayed", &WasmGameController::getSkipBoPlayed)
        // Legal moves
        .function("getLegalMoves", &WasmGameController::getLegalMoves)
        // Actions
        .function("applyMove", &WasmGameController::applyMove)
        .function("playAITurn", &WasmGameController::playAITurn)
        .function("playHeuristicAITurn", &WasmGameController::playHeuristicAITurn)
        .function("passTurn", &WasmGameController::passTurn)
        // Analysis
        .function("analyzeMoves", &WasmGameController::analyzeMoves)
        .function("analyzeChains", &WasmGameController::analyzeChains)
        .function("getMoveTree", &WasmGameController::getMoveTree)
        // Neural network
        .function("loadNNWeights", &WasmGameController::loadNNWeights)
        .function("hasNNWeights", &WasmGameController::hasNNWeights)
        .function("playNNAITurn", &WasmGameController::playNNAITurn)
        .function("analyzeNNChains", &WasmGameController::analyzeNNChains)
        ;
}
