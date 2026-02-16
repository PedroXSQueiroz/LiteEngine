#pragma once

#include <core/ui/UIRenderer.h>
#include <functional>
#include <vector>
#include <string>

namespace lite{

    template<typename T>
    concept UIRendererType = std::derived_from<T, UIRenderer<typename T::RendererType>>;


    template<UIRendererType URT>
    class UIElement{

    public:

        static const int EMPTY_ELEMENT_ID {-1};

        UIElement(URT* uiRenderer):m_uiRenderer(uiRenderer) {};

        virtual bool isFoccused() = 0;
        virtual ~UIElement() = default;
        virtual int draw(
            int parentId = EMPTY_ELEMENT_ID,
            int line = 0,
            int column = 0,
            int lineSpan = 0,
            int columnSpan = 0
        ) = 0;

        int getId() {return this->m_currentId;}
        void setId(int id) { this->m_currentId = id; }

    protected:

        std::vector<std::function<void(bool)>> onFoccusChange;

        URT* m_uiRenderer;
        int m_parentId{ EMPTY_ELEMENT_ID };
        int m_currentId{ EMPTY_ELEMENT_ID };

    };

    template<UIRendererType URT>
    class UIPanelElement : public UIElement<URT>{

    public:

        struct PanelGridCell {
        public:
            PanelGridCell(
                    UIElement<URT>* uiElememnt
                ,   int line
                ,   int column
                ,   int lineSpan = 0
                ,   int columnSpan = 0
            ):  m_uiElememnt(uiElememnt),
                m_line(line),
                m_column(column),
                m_lineSpan(lineSpan),
                m_columnSpan(columnSpan) {};

            int getColumn()     { return this->m_column; }
            int getLine()       { return this->m_line; }
            int getColumnSpan() { return this->m_columnSpan; }
            int getLineSpan()   { return this->m_lineSpan; }

            UIElement<URT>* getElement() { return this->m_uiElememnt; }

        private:

            int m_column;
            int m_line;
            int m_lineSpan;
            int m_columnSpan;

            UIElement<URT>* m_uiElememnt;
        };

        UIPanelElement(URT* renderer):
            UIElement<URT>(renderer)
        ,   m_childElements(std::vector<PanelGridCell>())
        {};

        virtual int drawContainer(
            int parentId,
            int line,
            int column,
            int lineSpan = 0,
            int columnSpan = 0
        ) = 0;

        void addChildComponent(UIElement<URT>* element,
            int line,
            int column,
            int lineSpan = 0,
            int columnSpan = 0
        ){

            this->addChildComponent(
                PanelGridCell(
                    element,
                    line,
                    column,
                    lineSpan,
                    columnSpan
                )
            );

        }

        void addChildComponent(PanelGridCell cell){
            this->m_childElements.push_back(cell);
        }

        virtual int draw(
            int parentId = UIElement<URT>::EMPTY_ELEMENT_ID,
            int line = 0,
            int column = 0,
            int lineSpan = 0,
            int columnSpan = 0
        ){

            int idContainer = drawContainer(
                parentId,
                line,
                column,
                lineSpan,
                columnSpan
            );

            for(PanelGridCell currentInnerElement : m_childElements)
            {
                if(currentInnerElement.getElement()->getId() == UIElement<URT>::EMPTY_ELEMENT_ID)
                {
                    currentInnerElement.getElement()->setId(this->m_uiRenderer->nextElementId());
                }

                int idInnerElement = currentInnerElement.getElement()->draw(
                        this->m_currentId
                    ,   currentInnerElement.getLine()
                    ,   currentInnerElement.getColumn()
                    ,   currentInnerElement.getLineSpan()
                    ,   currentInnerElement.getColumnSpan()
                );
            }

            return idContainer;
        };

    protected:

        std::vector<PanelGridCell> m_childElements;
    };

    template<UIRendererType URT>
    class UITextElement : public UIElement<URT>{

    public:

        UITextElement(URT* renderer): UIElement<URT>(renderer) {};

        virtual std::string getText() = 0;
        virtual bool setText(std::string text) = 0;

    };

    template<UIRendererType URT>
    class UICheckBoxElement : public UIElement<URT>{

    public:

        UICheckBoxElement(URT* uiRenderer): UIElement<URT>(uiRenderer) {};

        virtual bool isChecked() = 0;
        virtual bool setChecked(bool check) = 0;

    protected:

        std::vector<std::function<void(bool)>> onCheckValueChange;

    };

    template<UIRendererType URT>
    class UIComboBoxInputElement : public UIElement<URT>{

    public:

        UIComboBoxInputElement(URT* uiRenderer): UIElement<URT>(uiRenderer) {};

        virtual bool addOption(std::string key, std::string label) = 0;
        virtual std::string getSelectedOption() = 0;

        bool setSelectedOption(std::string key){
            if( this->updateInput(key) ) {
                this->notifyChange();
                return true;
            }
            return false;
        };

    protected:

        void notifyChange(){
            std::string currentSelectedOption = this->getSelectedOption();

            for(std::function<void(std::string)> currentCallback : this->onSelectValueChange){
                currentCallback(currentSelectedOption);
            }
        };

        virtual bool updateInput(std::string key) = 0;
        std::vector<std::function<void(std::string)>> onSelectValueChange;

    };

    template<UIRendererType URT>
    class UITextInputElement : public UIElement<URT>{

    public:

        UITextInputElement(URT* uiRenderer): UIElement<URT>(uiRenderer) {};

        virtual std::string getText() = 0;
        bool setText(std::string text){

            if(this->updateInput(text)){
                this->notifyChange(text);
                return true;
            }

            return false;
        };

        void notifyChange(std::string text = ""){

            if(text == "")
            {
                text = this->getText();
            }

            for( std::function<void(std::string)> currentCallback : this->onTextChange ){
                currentCallback(text);
            }
        };

    protected:

        virtual bool updateInput(std::string text) = 0;
        std::vector<std::function<void(std::string)>> onTextChange;

    };

    template<UIRendererType URT>
    class UITabElement : public UIElement<URT>{

    };

    template<UIRendererType URT>
    class UIButtonElement : public UIElement<URT>{

    public:

        UIButtonElement(URT* uiRenderer): UIElement<URT>(uiRenderer) {};

        void onClick(){
            for(std::function<void(UIButtonElement*)> currentCallback : this->onClickCallbacks){
                currentCallback(this);
            }
        }

        std::vector<std::function<void(UIButtonElement*)>> onClickCallbacks;

    };

}
