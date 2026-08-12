#pragma once

#include <vector>
#include <memory>

namespace lite {

// Base NÃO-template da hierarquia de nós instanciados.
//
// Existe SÓ para dar um tipo comum aos elos da árvore. Asset3dInstance é
// template em <Transform, DTO>, e instanciações com argumentos diferentes são
// tipos SEM relação de herança entre si (invariância de template) — mesmo que
// os argumentos sejam parentes. Sem esta base, o vetor de filhos não
// conseguiria guardar uma mesh e uma câmera ao mesmo tempo, porque os DTOs
// delas diferem.
//
// Nada além dos elos mora aqui: quem percorre a árvore por Node* e precisa de
// id, isMesh, transform etc. faz o downcast para o tipo concreto.
class Node {
public:
    // OBRIGATORIAMENTE virtual: children guarda unique_ptr<Node>, então a
    // destruição acontece por Node*. Sem isto os destrutores das derivadas não
    // rodam e os recursos de backend vazam.
    virtual ~Node() = default;

    // Hierarchy
    Node* parent = nullptr;  // Raw pointer (does not own)
    std::vector<std::unique_ptr<Node>> children;

    // Add child node with automatic parent assignment
    void addChild(Node* instance3d) {
        std::unique_ptr<Node> child(instance3d);
        child->parent = this;
        children.push_back(std::move(child));

        //TODO: ADICIONAR MÉTODO VIRTUAL AQUI. NO FILHO (FILAMENT) ELE DEVE PARENTEAR OS ENTITTIES DA FILAMENT.
    };
};

} // namespace lite
