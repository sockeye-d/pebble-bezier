// static void default_settings() {
//     settings.background_color = GColorBlack;
//
//     settings.hour_hand_length = 35;
//     settings.minute_hand_length = 50;
//     settings.hands_thickness = 2;
//     settings.hands_color = GColorWhite;
//
//     settings.ticks_thickness = 2;
//     settings.ticks_overlay_thickness = 10;
//     settings.ticks_color = GColorDarkGray;
//
//     settings.battery_indicator_thickness = 3;
//     settings.high_battery_color = GColorScreaminGreen;
//     settings.medium_battery_color = GColorChromeYellow;
//     settings.low_battery_color = GColorFolly;
// }
module.exports = [
    {
        type: "heading",
        defaultValue: "App Configuration"
    },
    {
        type: "color",
        messageKey: "background_color",
        defaultValue: "0x000000",
        label: "Background color",
    },
    {
        "type": "section",
        "items": [
            {
                type: "heading",
                defaultValue: "Hands",
            },
            {
                type: "slider",
                messageKey: "hour_hand_length",
                defaultValue: "35",
                label: "Hour hand length",
            },
            {
                type: "slider",
                messageKey: "minute_hand_length",
                defaultValue: "50",
                label: "Minute hand length",
            },
            {
                type: "slider",
                messageKey: "hands_thickness",
                defaultValue: "2",
                label: "Hands thickness",
            },
            {
                type: "color",
                messageKey: "hands_color",
                defaultValue: "0xFFFFFF",
                label: "Hands color",
            },
        ],
    },
    {
        "type": "section",
        "items": [
            {
                type: "heading",
                defaultValue: "Ticks",
            },
            {
                type: "slider",
                messageKey: "ticks_thickness",
                defaultValue: "2",
                label: "Tick thickness",
            },
            {
                type: "slider",
                messageKey: "ticks_overlay_thickness",
                defaultValue: "10",
                label: "Tick overlay thickness",
            },
            {
                type: "color",
                messageKey: "ticks_color",
                defaultValue: "0x555555",
                label: "Ticks color",
            },
        ],
    },
    {
        "type": "section",
        "items": [
            {
                type: "heading",
                defaultValue: "Battery indicator",
            },
            {
                type: "slider",
                messageKey: "battery_indicator_thickness",
                defaultValue: "3",
                label: "Battery ring thickness",
            },
            {
                type: "color",
                messageKey: "high_battery_color",
                defaultValue: "0x55FF55",
                label: "High battery color",
            },
            {
                type: "color",
                messageKey: "medium_battery_color",
                defaultValue: "0xFFAA00",
                label: "Medium battery color",
            },
            {
                type: "color",
                messageKey: "low_battery_color",
                defaultValue: "0xFF0055",
                label: "Low battery color",
            },
        ],
    },
    {
        "type": "submit",
        "defaultValue": "Save Settings"
    },
];
