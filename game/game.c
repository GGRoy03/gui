internal void 
GameEntryPoint()
{
    game_state State = {0};

    while(1)
    {
        OSWindow_Status WindowStatus = OSUpdateWindow();

        if(WindowStatus == OSWindow_Exit)
        {
            break;
        }
        else if(WindowStatus == OSWindow_Resize)
        {
            // TODO: Resizing.
        }

        BeginRendererFrame(&State.RenderContext);

        OSSleep(5);
    }
}
