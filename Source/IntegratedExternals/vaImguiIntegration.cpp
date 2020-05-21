#include "vaImguiIntegration.h"

using namespace Vanilla;

#ifdef VA_IMGUI_INTEGRATION_ENABLED

bool Vanilla::ImguiEx_VectorOfStringGetter( void * strVectorPtr, int n, const char** out_text )
{
    *out_text = (*(const std::vector<std::string> *)strVectorPtr)[n].c_str();
    return true;
}

namespace Vanilla
{
    static char     c_popup_InputString_Label[128] = { 0 };
    static char     c_popup_InputString_Value[128] = { 0 };
    static bool     c_popup_InputString_JustOpened = false;

    bool                ImGuiEx_PopupInputStringBegin( const string & label, const string & initialValue )
    {
        if( c_popup_InputString_JustOpened )
        {
            assert( false );
            return false;
        }
        strncpy_s( c_popup_InputString_Label, label.c_str(), sizeof( c_popup_InputString_Label ) - 1 );
        strncpy_s( c_popup_InputString_Value, initialValue.c_str(), sizeof( c_popup_InputString_Value ) - 1 );

        ImGui::OpenPopup( c_popup_InputString_Label );
        c_popup_InputString_JustOpened = true;
        return true;
    }

    bool                ImGuiEx_PopupInputStringTick( string & outValue )
    {
        ImGui::SetNextWindowContentSize(ImVec2(300.0f, 0.0f));
        if( ImGui::BeginPopupModal( c_popup_InputString_Label ) )
        {
            if( c_popup_InputString_JustOpened )
            {
                ImGui::SetKeyboardFocusHere();
                c_popup_InputString_JustOpened = false;
            }

            bool enterClicked = ImGui::InputText( "New name", c_popup_InputString_Value, sizeof( c_popup_InputString_Value ), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue );
            string newName = c_popup_InputString_Value;
        
            if( enterClicked || ImGui::Button( "Accept" ) )
            {
                if( newName != "" )
                {
                    outValue = newName;
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return true;
                }
            }
            ImGui::SameLine();
            if( ImGui::Button( "Cancel" ) )
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        return false;
    }

    ImVec2 ImGuiEx_CalcSmallButtonSize( const char * label )
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

        return ImGui::CalcItemSize( ImVec2(0, 0), label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f * 0.0f );
    }

    bool ImGuiEx_SmallButtonEx(const char* label, bool disabled)
    {
        ImGuiContext& g = *GImGui;
        float backup_padding_y = g.Style.FramePadding.y;
        g.Style.FramePadding.y = 0.0f;
        ImGuiButtonFlags flags = ImGuiButtonFlags_AlignTextBaseLine | ImGuiButtonFlags_PressedOnClick;
        if( disabled )
        {
            flags |= ImGuiButtonFlags_Disabled;
             ImGui::PushStyleColor( ImGuiCol_Text,           ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled) );
        }
        bool pressed = ImGui::ButtonEx(label, ImVec2(0, 0), flags);
        g.Style.FramePadding.y = backup_padding_y;
        if( disabled )
            ImGui::PopStyleColor( 1 );
        return pressed;
    }

    int ImGuiEx_SmallButtons( const char * keyID, const std::vector<string> & labels, const std::vector<bool> & disabled, bool verticalSeparator )
    {
        assert( labels.size() > 0 );
        assert( disabled.size( ) == 0 || disabled.size( ) == labels.size( ) );
        if( labels.size() == 0 )
            return -1;

        ImGui::PushID( keyID );

        float xSizeTotal = ImGui::GetStyle().ItemSpacing.x * (labels.size()-1);         // this measures additional spacing between items
        xSizeTotal += ((verticalSeparator)?(ImGui::GetStyle().ItemSpacing.x+1):(0));    // this measures spacing of vertical separator (if any)
        xSizeTotal -= ImGui::GetStyle().ItemSpacing.x;                                  // this pulls everything slightly to the right because reasons

        std::vector<float> xSizes(labels.size());

        for( size_t i = 0; i < labels.size(); i++ )
            xSizeTotal += xSizes[i] = ImGuiEx_CalcSmallButtonSize( labels[i].c_str() ).x;

        ImGui::SameLine( std::max( 0.0f, ImGui::GetContentRegionAvailWidth() - xSizeTotal ) );

        if( verticalSeparator )
        {
            ImGui::VerticalSeparator( ); ImGui::SameLine( );
        }

        int retVal = -1;
        for( size_t i = 0; i < labels.size( ); i++ )
        {
            if( ImGuiEx_SmallButtonEx( labels[i].c_str(), (disabled.size()==labels.size())?(disabled[i]):(false) ) )
                retVal = (int)i;
            if( (i+1) < labels.size() )
                ImGui::SameLine();
        }
        ImGui::PopID( );
        return retVal;
    }
}

#else
#endif

#ifdef VA_IMGUI_INTEGRATION_ENABLED

namespace Vanilla
{

    static ImFont *     s_bigClearSansRegular   = nullptr;
    static ImFont *     s_bigClearSansBold      = nullptr;

    ImFont *            ImGetBigClearSansRegular( )                 { return s_bigClearSansRegular; }
    ImFont *            ImGetBigClearSansBold( )                    { return s_bigClearSansBold;    }

    void                ImSetBigClearSansRegular( ImFont * font )   { s_bigClearSansRegular = font; }
    void                ImSetBigClearSansBold(    ImFont * font )   { s_bigClearSansBold    = font; }

}

#endif